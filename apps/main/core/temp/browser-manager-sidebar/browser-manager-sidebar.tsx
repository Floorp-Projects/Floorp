import "solid-xul/jsx-runtime";
import type { CBrowserManagerSidebar } from ".";

export function sidebar(bms: CBrowserManagerSidebar) {
  return (
    <>
      <xul:vbox
        id="sidebar2-box"
        style="min-width: 25em;"
        class="browser-sidebar2 chromeclass-extrachrome"
      >
        <xul:box id="sidebar2-header" style="min-height: 2.5em" align="center">
          <xul:toolbarbutton
            id="sidebar2-back"
            class="sidebar2-icon"
            style="margin-left: 0.5em;"
            data-l10n-id="sidebar-back-button"
            onClick={() => {
              bms.sidebarButtons("back");
            }}
          />
          <xul:toolbarbutton
            id="sidebar2-forward"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-forward-button"
            onClick={() => {
              bms.sidebarButtons("forward");
            }}
          />
          <xul:toolbarbutton
            id="sidebar2-reload"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-reload-button"
            onClick={() => {
              bms.sidebarButtons("reload");
            }}
          />
          <xul:toolbarbutton
            id="sidebar2-home"
            class="sidebar2-icon"
            style="margin-left: 1em;"
            data-l10n-id="sidebar-go-index-button"
            onClick={() => {
              bms.sidebarButtons("home");
            }}
          />
          <xul:spacer flex="1" />
          <xul:toolbarbutton
            id="sidebar2-keepPanelWidth"
            context="width-size-context"
            class="sidebar2-icon"
            style="margin-right: 0.5em;"
            data-l10n-id="sidebar-keepWidth-button"
            onClick={() => {
              bms.keepWebPanelWidth();
            }}
          />
          <xul:toolbarbutton
            id="sidebar2-close"
            class="sidebar2-icon"
            style="margin-right: 0.5em;"
            data-l10n-id="sidebar2-close-button"
            onClick={() => {
              bms.controlFunctions.changeVisibilityOfWebPanel();
            }}
          />
        </xul:box>
      </xul:vbox>
      <xul:splitter
        id="sidebar-splitter2"
        class="browser-sidebar2 chromeclass-extrachrome"
        hidden="false"
      />
    </>
  );
}
