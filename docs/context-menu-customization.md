<!-- SPDX-License-Identifier: MPL-2.0 -->

# Context Menu Customization

Open `about:hub#/features/context-menu` and enable context-menu customization.
Choose a browser surface and context profile, then hide commands with their
visibility switches or change their order.

## Arrange menu items

Open the native menu in the context you want to customize, such as a link, an
image, or selected text. Refresh the catalog in the settings page to discover
entries created dynamically.

The **Current context** view shows commands available in the most recently
observed context and supports drag and drop. The **All items** view includes
known commands that Firefox currently makes unavailable. Use the arrow buttons
or **Move to…** to place commands precisely among those entries; drag and drop
is disabled in this view. The move controls also support keyboard navigation.

## Share or separate layouts

Profiles share their surface layout by default. Enable independent settings to
keep a separate layout for links, images, selected text, or another context.
Returning to the shared layout retains the independent layout for later use.

**Reset profile** removes the selected profile's independent layout and returns
it to the shared layout. **Reset all context menus** removes custom layouts
across all surfaces and restores the default arrangement. Disabling the feature
restores the native menus while retaining your saved configuration.

## Scope and implementation

The feature affects browser-owned menus. Website menus, extension-owned items,
and protected native entries retain their own behavior. Configuration is saved
in `floorp.contextMenu.config`; `floorp.contextMenu.enabled` controls whether the
reversible visibility and ordering overlay is applied.

The runtime controller lives in
`browser-features/chrome/common/context-menu/controller.ts`; the editor lives in
`browser-features/pages-settings/src/app/context-menu/page.tsx`.
