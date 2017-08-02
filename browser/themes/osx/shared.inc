%include ../../../toolkit/themes/osx/global/shared.inc
%include ../shared/browser.inc

%filter substitution

%define hudButton -moz-appearance: none; color: #434343; border-radius: 4px; border: 1px solid #b5b5b5; background: linear-gradient(#fff, #f2f2f2); box-shadow: inset 0 1px rgba(255,255,255,.8), inset 0 0 1px rgba(255,255, 255,.25), 0 1px rgba(255,255,255,.3); background-clip: padding-box; background-origin: padding-box; padding: 2px 6px;
%define hudButtonPressed box-shadow: inset 0 1px 4px -3px #000, 0 1px rgba(255,255,255,.3);
