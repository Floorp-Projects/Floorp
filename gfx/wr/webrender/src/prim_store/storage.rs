/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{ops, marker::PhantomData, u32};
use crate::util::Recycler;

#[derive(Debug, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Index<T>(u32, PhantomData<T>);

// We explicitly implement Copy + Clone instead of using #[derive(Copy, Clone)]
// because we don't want to require that T implements Clone + Copy.
impl<T> Clone for Index<T> {
    fn clone(&self) -> Self { *self }
}

impl<T> Copy for Index<T> {}

impl<T> PartialEq for Index<T> {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl<T> Index<T> {
    fn new(idx: usize) -> Self {
        debug_assert!(idx < u32::max_value() as usize);
        Index(idx as u32, PhantomData)
    }

    pub const INVALID: Index<T> = Index(u32::MAX, PhantomData);
    pub const UNUSED: Index<T> = Index(u32::MAX-1, PhantomData);
}

#[derive(Debug)]
pub struct OpenRange<T> {
    start: Index<T>,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct Range<T> {
    pub start: Index<T>,
    pub end: Index<T>,
}

// We explicitly implement Copy + Clone instead of using #[derive(Copy, Clone)]
// because we don't want to require that T implements Clone + Copy.
impl<T> Clone for Range<T> {
    fn clone(&self) -> Self {
        Range { start: self.start, end: self.end }
    }
}
impl<T> Copy for Range<T> {}

impl<T> Range<T> {
    /// Create an empty `Range`
    pub fn empty() -> Self {
        Range {
            start: Index::new(0),
            end: Index::new(0),
        }
    }

    /// Check for an empty `Range`
    pub fn is_empty(self) -> bool {
        self.start.0 >= self.end.0
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct Storage<T> {
    data: Vec<T>,
}

impl<T> Storage<T> {
    pub fn new(initial_capacity: usize) -> Self {
        Storage {
            data: Vec::with_capacity(initial_capacity),
        }
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn clear(&mut self) {
        self.data.clear();
    }

    pub fn push(&mut self, t: T) -> Index<T> {
        let index = self.data.len();
        self.data.push(t);
        Index(index as u32, PhantomData)
    }

    pub fn reserve(&mut self, count: usize) {
        self.data.reserve(count);
    }

    pub fn recycle(&mut self, recycler: &mut Recycler) {
        recycler.recycle_vec(&mut self.data);
    }

    pub fn extend<II: IntoIterator<Item=T>>(&mut self, iter: II) -> Range<T> {
        let range = self.open_range();
        self.data.extend(iter);

        self.close_range(range)
    }

    pub fn open_range(&self) -> OpenRange<T> {
        OpenRange {
            start: Index::new(self.data.len())
        }
    }

    pub fn close_range(&self, range: OpenRange<T>) -> Range<T> {
        Range {
            start: range.start,
            end: Index::new(self.data.len()),
        }
    }
}

impl<T> ops::Index<Index<T>> for Storage<T> {
    type Output = T;
    fn index(&self, index: Index<T>) -> &Self::Output {
        &self.data[index.0 as usize]
    }
}

impl<T> ops::IndexMut<Index<T>> for Storage<T> {
    fn index_mut(&mut self, index: Index<T>) -> &mut Self::Output {
        &mut self.data[index.0 as usize]
    }
}

impl<T> ops::Index<Range<T>> for Storage<T> {
    type Output = [T];
    fn index(&self, index: Range<T>) -> &Self::Output {
        let start = index.start.0 as _;
        let end = index.end.0 as _;
        &self.data[start..end]
    }
}

impl<T> ops::IndexMut<Range<T>> for Storage<T> {
    fn index_mut(&mut self, index: Range<T>) -> &mut Self::Output {
        let start = index.start.0 as _;
        let end = index.end.0 as _;
        &mut self.data[start..end]
    }
}
