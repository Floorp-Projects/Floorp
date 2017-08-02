/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%filter substitution

%define toolbarHighlight hsla(0,0%,100%,.15)
%define toolbarHighlightLWT rgba(255,255,255,.4)
/* navbarInsetHighlight is tightly coupled to the toolbarHighlight constant. */
%define navbarInsetHighlight hsla(0,0%,100%,.4)
%define fgTabTexture linear-gradient(transparent 2px, @toolbarHighlight@ 2px, @toolbarHighlight@)
%define fgTabTextureLWT linear-gradient(transparent 2px, @toolbarHighlightLWT@ 2px, @toolbarHighlightLWT@)
%define fgTabBackgroundColor -moz-dialog
