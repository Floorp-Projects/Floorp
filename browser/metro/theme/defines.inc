%filter substitution

%define panel_dark_color #41464C
%define panel_dark_background url(chrome://browser/skin/images/panel-dark.png)
%define panel_light_color #F1F1F1

%define field_sizing 30px
%define field_foreground_color #000000
%define field_disabled_foreground_color #999999
%define field_mid_foreground_color #bababa
%define field_background_color #FFFFFF

%define border_color #BFC6CE
%define selected_color #FF8000

%define urlbar_border_color #BFC6CC
%define urlbar_edit_height 36px

%define toolbar_vertical_spacing 5px
%define toolbar_horizontal_spacing 20px
%define toolbar_snapped_horizontal_spacing 10px
%define toolbar_height 68px
%define labelled_toolbar_height 90px
%define tabs_height 178px
%define findbar_height 54px

%define progress_height 5px

%define metro_orange #FF8000

%define thumbnail_width 232px
%define thumbnail_height 148px

%define grid_column_width 131px
%define grid_double_column_width 262px
%define grid_row_height 86px
%define grid_double_row_height 172px

%define compactgrid_column_width 62px
%define compactgrid_row_height 62px

%define tile_border_color #dbdcde
%define tile_spacing 12px

%define scroller_thickness 4px
%define scroller_minimum 8px

%define metro_spacing_xsmall 3px
%define metro_spacing_small 5px
%define metro_spacing_snormal 10px
%define metro_spacing_normal 15px
%define metro_spacing_xnormal 20px
%define metro_spacing_xxnormal 25px
%define metro_spacing_xxxnormal 30px
%define metro_spacing_large 40px

%define metro_border_thin 1px
%define metro_border_thick 2px
%define metro_border_xthick 4px

%define metro_font_snormal 14px
%define metro_font_normal 15px
%define metro_font_large 25px
%define metro_font_xlarge 45px

%define font_normal 16px
%define font_snormal 15px
%define font_small 12px
%define font_xsmall 11px

%define touch_row 47px
%define touch_button_small 37px
%define touch_action_minwidth 133px

%define margin_large 16px
%define margin_xnormal 7px
%define margin_normal 5px

%define padding_xxnormal 8px
%define padding_xnormal 7px
%define padding_normal 5px

%define border_width_large 2px
%define border_width_small 1px
%define border_width_tiny 1px

%define border_radius_normal 5px
%define border_radius_tiny 1px

%define shadow_width_small 1px

% easing function and duration for animations match winJS showPanel method
%define metro_animation_duration 550ms
%define metro_animation_easing cubic-bezier(0.1, 0.9, 0.2, 1)

% this is meant to be slower than the system keyboard such
% that the bars slide up after the keyboard is in place.
%define appbar_keyboard_slideup_duration 750ms

% minimum resolution cutoffs for displaying 1.4x and 1.8x versions of icons
% XXX currently, there's some weirdness with the dppx unit, as documented in
% bug 895277. Because of that, we have to use 1.39dppx instead of 1.4dppx.
%define min_res_140pc 1.39dppx
%define min_res_180pc 1.8dppx