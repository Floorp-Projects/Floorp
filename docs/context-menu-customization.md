<!-- SPDX-License-Identifier: MPL-2.0 -->

# Context Menu Customization

The settings are available to participants in the `context_menu_customization`
Floorp Flasco with the `enabled` variant. When available, open
`about:hub#/features/context-menu` and enable context-menu customization. Choose
a browser surface and context profile, then hide commands with their visibility
switches or change their order.

## Flasco visibility

The sidebar entry, settings search results, and direct settings route use the
same participation check. Non-participants, disabled or inactive experiments,
control variants, and the **Never participate** policy hide the settings. A
direct visit then returns to the Hub home page. Failed participation checks also
keep the settings hidden.

Changing participation updates the settings visibility. This is a display gate:
saved menu customizations and their enabled preference are retained, and the
browser continues applying an already enabled layout even when the settings are
hidden.

A reference manifest entry is provided in
[`experiments.sample.json`](../browser-features/chrome/common/context-menu/experiments.sample.json).
It starts at zero rollout and can be enabled through the Flasco participation
controls after the entry is published. The sample is not deployed automatically;
the experiment must be added to the server's Flasco manifest to become
available.

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
in `floorp.contextMenu.config`; `floorp.contextMenu.enabled` controls whether
the reversible visibility and ordering overlay is applied.

The runtime controller lives in
`browser-features/chrome/common/context-menu/controller.ts`; the editor lives in
`browser-features/pages-settings/src/app/context-menu/page.tsx`.
