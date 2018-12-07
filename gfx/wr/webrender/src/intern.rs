/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{LayoutPrimitiveInfo, LayoutRect};
use internal_types::FastHashMap;
use profiler::ResourceProfileCounter;
use std::fmt::Debug;
use std::hash::Hash;
use std::marker::PhantomData;
use std::{mem, ops, u64};
use std::sync::atomic::{AtomicUsize, Ordering};
use util::VecHelper;

/*

 The interning module provides a generic data structure
 interning container. It is similar in concept to a
 traditional string interning container, but it is
 specialized to the WR thread model.

 There is an Interner structure, that lives in the
 scene builder thread, and a DataStore structure
 that lives in the frame builder thread.

 Hashing, interning and handle creation is done by
 the interner structure during scene building.

 Delta changes for the interner are pushed during
 a transaction to the frame builder. The frame builder
 is then able to access the content of the interned
 handles quickly, via array indexing.

 Epoch tracking ensures that the garbage collection
 step which the interner uses to remove items is
 only invoked on items that the frame builder thread
 is no longer referencing.

 Items in the data store are stored in a traditional
 free-list structure, for content access and memory
 usage efficiency.

 */

/// The epoch is incremented each time a scene is
/// built. The most recently used scene epoch is
/// stored inside each item and handle. This is
/// then used for cache invalidation (item) and
/// correctness validation (handle).
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, PartialEq)]
struct Epoch(u64);

impl Epoch {
    pub const INVALID: Self = Epoch(u64::MAX);
}

/// A list of updates to be applied to the data store,
/// provided by the interning structure.
pub struct UpdateList<S> {
    /// The current epoch of the scene builder.
    epoch: Epoch,
    /// The additions and removals to apply.
    updates: Vec<Update>,
    /// Actual new data to insert.
    data: Vec<S>,
}

lazy_static! {
    static ref NEXT_UID: AtomicUsize = AtomicUsize::new(0);
}

/// A globally, unique identifier
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct ItemUid {
    uid: usize,
}

impl ItemUid {
    pub fn next_uid() -> ItemUid {
        let uid = NEXT_UID.fetch_add(1, Ordering::Relaxed);
        ItemUid { uid }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone)]
pub struct Handle<M: Copy> {
    index: u32,
    epoch: Epoch,
    uid: ItemUid,
    _marker: PhantomData<M>,
}

impl <M> Handle<M> where M: Copy {
    pub fn uid(&self) -> ItemUid {
        self.uid
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum UpdateKind {
    Insert,
    Remove,
    UpdateEpoch,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Update {
    index: usize,
    kind: UpdateKind,
}

/// The data item is stored with an epoch, for validating
/// correct access patterns.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct Item<T> {
    epoch: Epoch,
    data: T,
}

/// The data store lives in the frame builder thread. It
/// contains a free-list of items for fast access.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DataStore<S, T, M> {
    items: Vec<Item<T>>,
    _source: PhantomData<S>,
    _marker: PhantomData<M>,
}

impl<S, T, M> ::std::default::Default for DataStore<S, T, M> where S: Debug, T: From<S>, M: Debug
{
    fn default() -> Self {
        DataStore {
            items: Vec::new(),
            _source: PhantomData,
            _marker: PhantomData,
        }
    }
}

impl<S, T, M> DataStore<S, T, M> where S: Debug, T: From<S>, M: Debug
{
    /// Apply any updates from the scene builder thread to
    /// this data store.
    pub fn apply_updates(
        &mut self,
        update_list: UpdateList<S>,
        profile_counter: &mut ResourceProfileCounter,
    ) {
        let mut data_iter = update_list.data.into_iter();
        for update in update_list.updates {
            match update.kind {
                UpdateKind::Insert => {
                    self.items.entry(update.index).set(Item {
                        data: T::from(data_iter.next().unwrap()),
                        epoch: update_list.epoch,
                    });
                }
                UpdateKind::Remove => {
                    self.items[update.index].epoch = Epoch::INVALID;
                }
                UpdateKind::UpdateEpoch => {
                    self.items[update.index].epoch = update_list.epoch;
                }
            }
        }

        let per_item_size = mem::size_of::<S>() + mem::size_of::<T>();
        profile_counter.set(self.items.len(), per_item_size * self.items.len());

        debug_assert!(data_iter.next().is_none());
    }
}

/// Retrieve an item from the store via handle
impl<S, T, M> ops::Index<Handle<M>> for DataStore<S, T, M>
where M: Copy
{
    type Output = T;
    fn index(&self, handle: Handle<M>) -> &T {
        let item = &self.items[handle.index as usize];
        assert_eq!(item.epoch, handle.epoch);
        &item.data
    }
}

/// Retrieve a mutable item from the store via handle
/// Retrieve an item from the store via handle
impl<S, T, M> ops::IndexMut<Handle<M>> for DataStore<S, T, M>
where
    M: Copy
{
    fn index_mut(&mut self, handle: Handle<M>) -> &mut T {
        let item = &mut self.items[handle.index as usize];
        assert_eq!(item.epoch, handle.epoch);
        &mut item.data
    }
}

/// The main interning data structure. This lives in the
/// scene builder thread, and handles hashing and interning
/// unique data structures. It also manages a free-list for
/// the items in the data store, which is synchronized via
/// an update list of additions / removals.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Interner<S, D, M>
where
    S: Eq + Hash + Clone + Debug,
    M: Copy
{
    /// Uniquely map an interning key to a handle
    map: FastHashMap<S, Handle<M>>,
    /// List of free slots in the data store for re-use.
    free_list: Vec<usize>,
    /// Pending list of updates that need to be applied.
    updates: Vec<Update>,
    /// Pending new data to insert.
    update_data: Vec<S>,
    /// The current epoch for the interner.
    current_epoch: Epoch,
    /// The information associated with each interned
    /// item that can be accessed by the interner.
    local_data: Vec<Item<D>>,
}

impl<S, D, M> ::std::default::Default for Interner<S, D, M>
where
    S: Eq + Hash + Clone + Debug,
    M: Copy + Debug
{
    fn default() -> Self {
        Interner {
            map: FastHashMap::default(),
            free_list: Vec::new(),
            updates: Vec::new(),
            update_data: Vec::new(),
            current_epoch: Epoch(1),
            local_data: Vec::new(),
        }
    }
}

impl<S, D, M> Interner<S, D, M>
where
    S: Eq + Hash + Clone + Debug,
    M: Copy + Debug
{
    /// Intern a data structure, and return a handle to
    /// that data. The handle can then be stored in the
    /// frame builder, and safely accessed via the data
    /// store that lives in the frame builder thread.
    /// The provided closure is invoked to build the
    /// local data about an interned structure if the
    /// key isn't already interned.
    pub fn intern<F>(
        &mut self,
        data: &S,
        f: F,
    ) -> Handle<M> where F: FnOnce() -> D {
        // Use get_mut rather than entry here to avoid
        // cloning the (sometimes large) key in the common
        // case, where the data already exists in the interner.
        if let Some(handle) = self.map.get_mut(data) {
            // Update the epoch in the data store. This
            // is not strictly needed for correctness, but
            // is used to ensure items are only accessed
            // via valid handles.
            if handle.epoch != self.current_epoch {
                self.updates.push(Update {
                    index: handle.index as usize,
                    kind: UpdateKind::UpdateEpoch,
                });
                self.local_data[handle.index as usize].epoch = self.current_epoch;
            }
            handle.epoch = self.current_epoch;
            return *handle;
        }

        // We need to intern a new data item. First, find out
        // if there is a spare slot in the free-list that we
        // can use. Otherwise, append to the end of the list.
        let index = match self.free_list.pop() {
            Some(index) => index,
            None => self.local_data.len(),
        };

        // Add a pending update to insert the new data.
        self.updates.push(Update {
            index,
            kind: UpdateKind::Insert,
        });
        self.update_data.alloc().init(data.clone());

        // Generate a handle for access via the data store.
        let handle = Handle {
            index: index as u32,
            epoch: self.current_epoch,
            uid: ItemUid::next_uid(),
            _marker: PhantomData,
        };

        // Store this handle so the next time it is
        // interned, it gets re-used.
        self.map.insert(data.clone(), handle);

        // Create the local data for this item that is
        // being interned.
        self.local_data.entry(index).set(Item {
            epoch: self.current_epoch,
            data: f(),
        });

        handle
    }

    /// Retrieve the pending list of updates for an interner
    /// that need to be applied to the data store. Also run
    /// a GC step that removes old entries.
    pub fn end_frame_and_get_pending_updates(&mut self) -> UpdateList<S> {
        let mut updates = mem::replace(&mut self.updates, Vec::new());
        let data = mem::replace(&mut self.update_data, Vec::new());

        let free_list = &mut self.free_list;
        let current_epoch = self.current_epoch.0;

        // First, run a GC step. Walk through the handles, and
        // if we find any that haven't been used for some time,
        // remove them. If this ever shows up in profiles, we
        // can make the GC step partial (scan only part of the
        // map each frame). It also might make sense in the
        // future to adjust how long items remain in the cache
        // based on the current size of the list.
        self.map.retain(|_, handle| {
            if handle.epoch.0 + 10 < current_epoch {
                // To expire an item:
                //  - Add index to the free-list for re-use.
                //  - Add an update to the data store to invalidate this slow.
                //  - Remove from the hash map.
                free_list.push(handle.index as usize);
                updates.push(Update {
                    index: handle.index as usize,
                    kind: UpdateKind::Remove,
                });
                return false;
            }

            true
        });

        let updates = UpdateList {
            updates,
            data,
            epoch: self.current_epoch,
        };

        // Begin the next epoch
        self.current_epoch = Epoch(self.current_epoch.0 + 1);

        updates
    }
}

/// Retrieve the local data for an item from the interner via handle
impl<S, D, M> ops::Index<Handle<M>> for Interner<S, D, M>
where
    S: Eq + Clone + Hash + Debug,
    M: Copy + Debug
{
    type Output = D;
    fn index(&self, handle: Handle<M>) -> &D {
        let item = &self.local_data[handle.index as usize];
        assert_eq!(item.epoch, handle.epoch);
        &item.data
    }
}

/// Implement `Internable` for a type that wants participate in interning.
///
/// see DisplayListFlattener::add_interned_primitive<P>
pub trait Internable {
    type Marker: Copy + Debug;
    type Source: Eq + Hash + Clone + Debug;
    type StoreData: From<Self::Source>;
    type InternData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect,
    ) -> Self::Source;
}
