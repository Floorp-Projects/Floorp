/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::units::*;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{
    InternablePrimitive, PrimitiveInstanceKind, PrimKey, PrimTemplate,
    PrimTemplateCommonData, PrimitiveStore, PictureIndex,
};
use crate::scene_building::IsVisible;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, MallocSizeOf, Hash)]
pub struct BackdropCapture {
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, MallocSizeOf, Hash)]
pub struct BackdropRender {
}

impl From<BackdropCapture> for BackdropCaptureData {
    fn from(_backdrop: BackdropCapture) -> Self {
        BackdropCaptureData {
        }
    }
}

impl From<BackdropRender> for BackdropRenderData {
    fn from(_backdrop: BackdropRender) -> Self {
        BackdropRenderData {
        }
    }
}

pub type BackdropCaptureKey = PrimKey<BackdropCapture>;
pub type BackdropRenderKey = PrimKey<BackdropRender>;

impl BackdropCaptureKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        backdrop_capture: BackdropCapture,
    ) -> Self {
        BackdropCaptureKey {
            common: info.into(),
            kind: backdrop_capture,
        }
    }
}

impl BackdropRenderKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        backdrop_render: BackdropRender,
    ) -> Self {
        BackdropRenderKey {
            common: info.into(),
            kind: backdrop_render,
        }
    }
}

impl InternDebug for BackdropCaptureKey {}
impl InternDebug for BackdropRenderKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BackdropCaptureData {
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BackdropRenderData {
}

pub type BackdropCaptureTemplate = PrimTemplate<BackdropCaptureData>;
pub type BackdropRenderTemplate = PrimTemplate<BackdropRenderData>;

impl From<BackdropCaptureKey> for BackdropCaptureTemplate {
    fn from(backdrop: BackdropCaptureKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(backdrop.common);

        BackdropCaptureTemplate {
            common,
            kind: backdrop.kind.into(),
        }
    }
}

impl From<BackdropRenderKey> for BackdropRenderTemplate {
    fn from(backdrop: BackdropRenderKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(backdrop.common);

        BackdropRenderTemplate {
            common,
            kind: backdrop.kind.into(),
        }
    }
}

pub type BackdropCaptureDataHandle = InternHandle<BackdropCapture>;
pub type BackdropRenderDataHandle = InternHandle<BackdropRender>;

impl Internable for BackdropCapture {
    type Key = BackdropCaptureKey;
    type StoreData = BackdropCaptureTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_BACKDROP_CAPTURES;
}

impl Internable for BackdropRender {
    type Key = BackdropRenderKey;
    type StoreData = BackdropRenderTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_BACKDROP_RENDERS;
}

impl InternablePrimitive for BackdropCapture {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> BackdropCaptureKey {
        BackdropCaptureKey::new(info, self)
    }

    fn make_instance_kind(
        _key: BackdropCaptureKey,
        data_handle: BackdropCaptureDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::BackdropCapture {
            data_handle,
        }
    }
}

impl InternablePrimitive for BackdropRender {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> BackdropRenderKey {
        BackdropRenderKey::new(info, self)
    }

    fn make_instance_kind(
        _key: BackdropRenderKey,
        data_handle: BackdropRenderDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::BackdropRender {
            data_handle,
            pic_index: PictureIndex::INVALID,
        }
    }
}

impl IsVisible for BackdropCapture {
    fn is_visible(&self) -> bool {
        true
    }
}

impl IsVisible for BackdropRender {
    fn is_visible(&self) -> bool {
        true
    }
}
