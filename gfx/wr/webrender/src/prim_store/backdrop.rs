/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::PrimitiveFlags;
use api::units::*;
use crate::clip_scroll_tree::SpatialNodeIndex;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{
    InternablePrimitive, PictureIndex, PrimitiveInstanceKind, PrimKey, PrimKeyCommonData, PrimTemplate,
    PrimTemplateCommonData, PrimitiveStore, PrimitiveSceneData, RectangleKey,
};

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, MallocSizeOf, Hash)]
pub struct Backdrop {
    pub pic_index: PictureIndex,
    pub spatial_node_index: SpatialNodeIndex,
    pub border_rect: RectangleKey,
}

impl From<Backdrop> for BackdropData {
    fn from(backdrop: Backdrop) -> Self {
        BackdropData {
            pic_index: backdrop.pic_index,
            spatial_node_index: backdrop.spatial_node_index,
            border_rect: backdrop.border_rect.into(),
        }
    }
}

pub type BackdropKey = PrimKey<Backdrop>;

impl BackdropKey {
    pub fn new(
        flags: PrimitiveFlags,
        prim_size: LayoutSize,
        backdrop: Backdrop,
    ) -> Self {
        BackdropKey {
            common: PrimKeyCommonData {
                flags,
                prim_size: prim_size.into(),
            },
            kind: backdrop,
        }
    }
}

impl InternDebug for BackdropKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BackdropData {
    pub pic_index: PictureIndex,
    pub spatial_node_index: SpatialNodeIndex,
    pub border_rect: LayoutRect,
}

pub type BackdropTemplate = PrimTemplate<BackdropData>;

impl From<BackdropKey> for BackdropTemplate {
    fn from(backdrop: BackdropKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(backdrop.common);

        BackdropTemplate {
            common,
            kind: backdrop.kind.into(),
        }
    }
}

pub type BackdropDataHandle = InternHandle<Backdrop>;

impl Internable for Backdrop {
    type Key = BackdropKey;
    type StoreData = BackdropTemplate;
    type InternData = PrimitiveSceneData;
}

impl InternablePrimitive for Backdrop {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> BackdropKey {
        BackdropKey::new(
            info.flags,
            info.rect.size,
            self
        )
    }

    fn make_instance_kind(
        _key: BackdropKey,
        data_handle: BackdropDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::Backdrop {
            data_handle,
        }
    }
}
