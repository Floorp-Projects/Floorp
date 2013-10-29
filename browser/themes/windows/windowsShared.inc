/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%filter substitution

%define toolbarHighlight rgba(253,253,253,0.45)
%define fgTabTexture linear-gradient(transparent, transparent 2px, rgba(254,254,254,0.72) 2px, rgba(254,254,254,0.72) 2px, rgba(250,250,250,0.88) 3px, rgba(250,250,250,0.88) 3px, rgba(254,254,254,0.72) 4px, rgba(254,254,254,0.72) 4px, @toolbarHighlight@)
%define fgTabBackgroundColor -moz-dialog
%define fgTabTextureLWT @fgTabTexture@

% Aero-only defines
%define customToolbarColor hsl(210,75%,92%)
