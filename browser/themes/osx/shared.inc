%include ../../../toolkit/themes/osx/global/shared.inc
%include ../shared/browser.inc

%filter substitution

%define fgTabTexture linear-gradient(transparent 2px, hsl(0,0%,99%) 2px, hsl(0,0%,93%))
%define fgTabTextureYosemiteInactive linear-gradient(transparent 2px, hsl(0,0%,99%) 2px, hsl(0,0%,97%))
%define toolbarColorLWT rgba(253,253,253,0.45)
%define fgTabTextureLWT linear-gradient(transparent 2px, rgba(254,254,254,.72) 2px, @toolbarColorLWT@)
%define fgTabBackgroundColor transparent
%define hudButton -moz-appearance: none; color: #434343; border-radius: 4px; border: 1px solid #b5b5b5; background: linear-gradient(#fff, #f2f2f2); box-shadow: inset 0 1px rgba(255,255,255,.8), inset 0 0 1px rgba(255,255, 255,.25), 0 1px rgba(255,255,255,.3); background-clip: padding-box; background-origin: padding-box; padding: 2px 6px;
%define hudButtonPressed box-shadow: inset 0 1px 4px -3px #000, 0 1px rgba(255,255,255,.3);
%define hudButtonFocused box-shadow: 0 0 1px -moz-mac-focusring inset, 0 0 4px 1px -moz-mac-focusring, 0 0 2px 1px -moz-mac-focusring;
