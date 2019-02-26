/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

macro_rules! common {
    () => (
        use ::intern;
        #[allow(unused_imports)]
        use ::prim_store::PrimitiveSceneData;
        #[cfg_attr(feature = "capture", derive(Serialize))]
        #[cfg_attr(feature = "replay", derive(Deserialize))]
        #[derive(Clone, Copy, Debug, Eq, Hash, MallocSizeOf, PartialEq)]
        pub struct Marker;
        pub type Handle = intern::Handle<Marker>;
    )
}

pub mod clip {
common!();
use ::clip::{ClipItemKey, ClipNode};
pub type Store = intern::DataStore<ClipItemKey, ClipNode, Marker>;
pub type UpdateList = intern::UpdateList<ClipItemKey>;
pub type Interner = intern::Interner<ClipItemKey, (), Marker>;
}

pub mod prim {
common!();
use ::prim_store::{PrimitiveKey, PrimitiveTemplate};
pub type Store = intern::DataStore<PrimitiveKey, PrimitiveTemplate, Marker>;
pub type UpdateList = intern::UpdateList<PrimitiveKey>;
pub type Interner = intern::Interner<PrimitiveKey, PrimitiveSceneData, Marker>;
}

pub mod normal_border {
common!();
use ::prim_store::borders::{NormalBorderKey, NormalBorderTemplate};
pub type Store = intern::DataStore<NormalBorderKey, NormalBorderTemplate, Marker>;
pub type UpdateList = intern::UpdateList<NormalBorderKey>;
pub type Interner = intern::Interner<NormalBorderKey, PrimitiveSceneData, Marker>;
}

pub mod image_border {
common!();
use ::prim_store::borders::{ImageBorderKey, ImageBorderTemplate};
pub type Store = intern::DataStore<ImageBorderKey, ImageBorderTemplate, Marker>;
pub type UpdateList = intern::UpdateList<ImageBorderKey>;
pub type Interner = intern::Interner<ImageBorderKey, PrimitiveSceneData, Marker>;
}

pub mod image {
common!();
use ::prim_store::image::{ImageKey, ImageTemplate};
pub type Store = intern::DataStore<ImageKey, ImageTemplate, Marker>;
pub type UpdateList = intern::UpdateList<ImageKey>;
pub type Interner = intern::Interner<ImageKey, PrimitiveSceneData, Marker>;
}

pub mod yuv_image {
common!();
use ::prim_store::image::{YuvImageKey, YuvImageTemplate};
pub type Store = intern::DataStore<YuvImageKey, YuvImageTemplate, Marker>;
pub type UpdateList = intern::UpdateList<YuvImageKey>;
pub type Interner = intern::Interner<YuvImageKey, PrimitiveSceneData, Marker>;
}

pub mod line_decoration {
use ::prim_store::line_dec::{LineDecorationKey, LineDecorationTemplate};
common!();
pub type Store = intern::DataStore<LineDecorationKey, LineDecorationTemplate, Marker>;
pub type UpdateList = intern::UpdateList<LineDecorationKey>;
pub type Interner = intern::Interner<LineDecorationKey, PrimitiveSceneData, Marker>;
}

pub mod linear_grad {
common!();
use ::prim_store::gradient::{LinearGradientKey, LinearGradientTemplate};
pub type Store = intern::DataStore<LinearGradientKey, LinearGradientTemplate, Marker>;
pub type UpdateList = intern::UpdateList<LinearGradientKey>;
pub type Interner = intern::Interner<LinearGradientKey, PrimitiveSceneData, Marker>;
}

pub mod radial_grad {
common!();
use ::prim_store::gradient::{RadialGradientKey, RadialGradientTemplate};
pub type Store = intern::DataStore<RadialGradientKey, RadialGradientTemplate, Marker>;
pub type UpdateList = intern::UpdateList<RadialGradientKey>;
pub type Interner = intern::Interner<RadialGradientKey, PrimitiveSceneData, Marker>;
}

pub mod picture {
common!();
use ::prim_store::picture::{PictureKey, PictureTemplate};
pub type Store = intern::DataStore<PictureKey, PictureTemplate, Marker>;
pub type UpdateList = intern::UpdateList<PictureKey>;
pub type Interner = intern::Interner<PictureKey, PrimitiveSceneData, Marker>;
}

pub mod text_run {
common!();
use ::prim_store::text_run::{TextRunKey, TextRunTemplate};
pub type Store = intern::DataStore<TextRunKey, TextRunTemplate, Marker>;
pub type UpdateList = intern::UpdateList<TextRunKey>;
pub type Interner = intern::Interner<TextRunKey, PrimitiveSceneData, Marker>;
}



