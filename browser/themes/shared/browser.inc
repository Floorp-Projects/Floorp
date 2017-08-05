%filter substitution

% Note that zoom-reset-button is a bit different since it doesn't use an image and thus has the image with display: none.
%define nestedButtons #zoom-out-button, #zoom-reset-button, #zoom-in-button, #cut-button, #copy-button, #paste-button

%define inAnyPanel :-moz-any(:not([cui-areatype="toolbar"]), [overflowedItem=true])

%define panelPaletteIconSize var(--panel-palette-icon-size)