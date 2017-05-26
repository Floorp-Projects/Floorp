/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%filter substitution

%define toolbarHighlight rgba(255,255,255,.4)
%define fgTabTexture linear-gradient(transparent 2px, @toolbarHighlight@ 2px, @toolbarHighlight@)
%define fgTabBackgroundColor -moz-dialog
%define fgTabTextureLWT @fgTabTexture@
%ifndef MOZ_PHOTON_THEME
%define customToolbarColor hsl(210,75%,92%)
%endif
