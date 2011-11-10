%include ../../../toolkit/themes/pinstripe/global/shared.inc
%include ../browserShared.inc

%define hudButton -moz-appearance: none; color: #fff; text-shadow: 0 -1px 0 rgba(0,0,0,.5); border-radius: 12px; border: 1px solid rgba(0,0,0,.65); background: -moz-linear-gradient(rgba(110,110,110,.9), rgba(70,70,70,.9) 49%, rgba(50,50,50,.9) 51%, rgba(40,40,40,.9)); box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 0 1px rgba(255,255,255,.1), 0 1px 0 rgba(255,255,255,.1); background-clip: padding-box; background-origin: padding-box; padding: 2px 9px;
%define hudButtonPressed background: -moz-linear-gradient(rgba(40,40,40,.9), rgba(70,70,70,.9)); box-shadow: inset 0 0 3px rgba(0,0,0,.2), inset 0 1px 7px rgba(0,0,0,.4), 0 1px 0 rgba(255,255,255,.1);
%define hudButtonFocused box-shadow: 0 0 1px -moz-mac-focusring inset, 0 0 4px 1px -moz-mac-focusring, 0 0 2px 1px -moz-mac-focusring;
