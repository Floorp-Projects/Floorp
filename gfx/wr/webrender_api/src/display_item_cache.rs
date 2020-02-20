/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::display_item::*;
use crate::display_list::*;

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
            DisplayItem::Rectangle(..) |
            DisplayItem::Image(..) => CachedDisplayItem {
                item: *item,
                data: Vec::new(),
            },
            _ => { unimplemented!("Unsupported display item type"); }
        }
    }
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct DisplayItemCache {
    items: Vec<Option<CachedDisplayItem>>
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

    pub fn add_item(
        &mut self,
        key: Option<ItemKey>,
        item: DisplayItemRef
    ) {
        let index = usize::from(key.expect("Cached item without key"));
        self.items[index] = Some(CachedDisplayItem::from(item));
    }

    pub fn get_item(
        &self,
        key: ItemKey
    ) -> Option<&CachedDisplayItem> {
        self.items[key as usize].as_ref()
    }

    pub fn update(
        &mut self,
        display_list: &BuiltDisplayList
    ) {
        self.grow_if_needed(display_list.cache_size());

        let mut iter = display_list.extra_data_iter();

        loop {
            let item = match iter.next() {
                Some(item) => item,
                None => break,
            };

            match item.item() {
                DisplayItem::Rectangle(ref info) => {
                    self.add_item(info.common.item_key, item);
                }
                DisplayItem::Text(ref info) => {
                    self.add_item(info.common.item_key, item);
                }
                DisplayItem::Image(ref info) => {
                    self.add_item(info.common.item_key, item);
                }
                item @ _ => {
                    unimplemented!("Unexpected item in extra data: {:?}", item);
                }
            }
        }
    }
}
