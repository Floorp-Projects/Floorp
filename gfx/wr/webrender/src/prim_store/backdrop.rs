/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::*;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{
    InternablePrimitive, PrimitiveInstanceKind, PrimKey, PrimTemplate,
    PrimTemplateCommonData, PrimitiveStore,
};
use crate::scene_building::IsVisible;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, MallocSizeOf, Hash)]
pub struct Backdrop {
}

impl From<Backdrop> for BackdropData {
    fn from(_backdrop: Backdrop) -> Self {
        BackdropData {
        }
    }
}

pub type BackdropKey = PrimKey<Backdrop>;

impl BackdropKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        backdrop: Backdrop,
    ) -> Self {
        BackdropKey {
            common: info.into(),
            kind: backdrop,
        }
    }
}

impl InternDebug for BackdropKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BackdropData {
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
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_BACKDROPS;
}

impl InternablePrimitive for Backdrop {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> BackdropKey {
        BackdropKey::new(info, self)
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

impl IsVisible for Backdrop {
    fn is_visible(&self) -> bool {
        true
    }
}
