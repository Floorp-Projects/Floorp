/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%filter substitution

%define toolbarHighlight rgba(255,255,255,.3)
%define fgTabTexture linear-gradient(transparent 0px, transparent 2px, hsla(0,0%,100%,0.35) 2px, hsla(0,0%,100%,0.35) 3px, hsla(0,0%,100%,0.65) 3px, hsla(0,0%,100%,0.65) 4px, @toolbarHighlight@)
%define fgTabTextureLWT @fgTabTexture@
%define fgTabBackgroundColor -moz-dialog
