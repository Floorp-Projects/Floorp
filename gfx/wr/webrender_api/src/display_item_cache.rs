/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::display_item::*;
use crate::display_list::*;
#[cfg(debug_assertions)]
use std::collections::HashSet;

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct CachedDisplayItem {
    item: DisplayItem,
    data: Vec<u8>,
}

impl CachedDisplayItem {
    pub fn item(&self) -> &DisplayItem {
        &self.item
    }

    pub fn data_as_item_range<T>(&self) -> ItemRange<T> {
        ItemRange::new(&self.data)
    }
}

impl From<DisplayItemRef<'_, '_>> for CachedDisplayItem {
    fn from(item_ref: DisplayItemRef) -> Self {
        let item = item_ref.item();

        match item {
            DisplayItem::Text(..) => CachedDisplayItem {
                item: *item,
                data: item_ref.glyphs().bytes().to_vec(),
            },
            _ => CachedDisplayItem {
                item: *item,
                data: Vec::new(),
            },
        }
    }
}

fn key_from_item(item: &DisplayItem) -> ItemKey {
    let key = match item {
        DisplayItem::Rectangle(ref info) => info.common.item_key,
        DisplayItem::ClearRectangle(ref info) => info.common.item_key,
        DisplayItem::HitTest(ref info) => info.common.item_key,
        DisplayItem::Text(ref info) => info.common.item_key,
        DisplayItem::Image(ref info) => info.common.item_key,
        _ => unimplemented!("Unexpected item: {:?}", item)
    };

    key.expect("Cached item without a key")
}

#[derive(Clone, Deserialize, Serialize)]
pub struct DisplayItemCache {
    items: Vec<Option<CachedDisplayItem>>,

    #[cfg(debug_assertions)]
    keys: HashSet<ItemKey>,
}

impl DisplayItemCache {
    fn grow_if_needed(
        &mut self,
        capacity: usize
    ) {
        if capacity > self.items.len() {
            self.items.resize_with(capacity, || None::<CachedDisplayItem>);
            // println!("Current cache size: {:?}",
            //     mem::size_of::<CachedDisplayItem>() * capacity);
        }
    }

    fn add_item(
        &mut self,
        item: CachedDisplayItem,
        key: ItemKey,
    ) {
        self.items[key as usize] = Some(item);
    }

    pub fn get_item(
        &self,
        key: ItemKey
    ) -> Option<&CachedDisplayItem> {
        self.items[key as usize].as_ref()
    }

    pub fn new() -> Self {
        Self {
            items: Vec::new(),

            #[cfg(debug_assertions)]
            /// Used to check that there is only one item per key.
            keys: HashSet::new(),
        }
    }

    pub fn update(
        &mut self,
        display_list: &BuiltDisplayList
    ) {
        self.grow_if_needed(display_list.cache_size());

        let mut iter = display_list.extra_data_iter();

        #[cfg(debug_assertions)]
        {
            self.keys.clear();
        }

        loop {
            let item = match iter.next() {
                Some(item) => item,
                None => break,
            };

            let item_key = key_from_item(item.item());
            let cached_item = CachedDisplayItem::from(item);

            #[cfg(debug_assertions)]
            {
                debug_assert!(self.keys.insert(item_key));
            }

            self.add_item(cached_item, item_key);
        }
    }
}
