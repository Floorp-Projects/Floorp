import "solid-xul/jsx-runtime";
import type { CBrowserManagerSidebar } from ".";

export function sidebarContext(bms: CBrowserManagerSidebar) {
  return (
    <xul:popupset>
      <xul:menupopup
        id="webpanel-context"
        onpopupshowing="gBrowserManagerSidebar.contextMenu.show(event);"
      >
        <xul:menuitem
          id="unloadWebpanelMenu"
          class="needLoadedWebpanel"
          data-l10n-id="sidebar2-unload-panel"
          label="Unload this webpanel"
          accesskey="U"
          onClick={() => {
            bms.contextMenu.unloadWebpanel();
          }}
        />
        <xul:menuseparator class="context-webpanel-separator" />
        <xul:menuitem
          id="muteMenu"
          class="needLoadedWebpanel"
          data-l10n-id="sidebar2-mute-and-unmute"
          label="Mute/Unmute this webpanel"
          accesskey="M"
          onClick={() => {
            bms.contextMenu.muteWebpanel();
          }}
        />
        <xul:menu
          id="changeZoomLevelMenu"
          class="needLoadedWebpanel needRunningExtensionsPanel"
          data-l10n-id="sidebar2-change-zoom-level"
          accesskey="Z"
        >
          <xul:menupopup id="changeZoomLevelPopup">
            <xul:menuitem
              id="zoomInMenu"
              accesskey="I"
              data-l10n-id="sidebar2-zoom-in"
              onClick={() => {
                bms.contextMenu.zoomIn();
              }}
            />
            <xul:menuitem
              id="zoomOutMenu"
              accesskey="O"
              data-l10n-id="sidebar2-zoom-out"
              onClick={() => {
                bms.contextMenu.zoomOut();
              }}
            />
            <xul:menuitem
              id="resetZoomMenu"
              accesskey="R"
              data-l10n-id="sidebar2-reset-zoom"
              onClick={() => {
                bms.contextMenu.resetZoom();
              }}
            />
          </xul:menupopup>
        </xul:menu>
        <xul:menuitem
          id="changeUAWebpanelMenu"
          data-l10n-id="sidebar2-change-ua-panel"
          label="Switch User agent to Mobile/Desktop Version at this Webpanel"
          accesskey="R"
          onClick={() => {
            bms.contextMenu.changeUserAgent();
          }}
        />
        <xul:menuseparator class="context-webpanel-separator" />
        <xul:menuitem
          id="deleteWebpanelMenu"
          data-l10n-id="sidebar2-delete-panel"
          accesskey="D"
          onClick={() => {
            bms.contextMenu.deleteWebpanel();
          }}
        />
      </xul:menupopup>

      <xul:menupopup
        id="all-panel-context"
        onpopupshowing="gBrowserManagerSidebar.contextMenu.show(event);"
      >
        <xul:menuitem
          id="unloadWebpanelMenu"
          class="needLoadedWebpanel"
          data-l10n-id="sidebar2-unload-panel"
          label="Unload this webpanel"
          accesskey="U"
          onClick={() => {
            bms.contextMenu.unloadWebpanel();
          }}
        />
        <xul:menuseparator class="context-webpanel-separator" />
        <xul:menuitem
          id="deleteWebpanelMenu"
          data-l10n-id="sidebar2-delete-panel"
          accesskey="D"
          onClick={() => {
            bms.contextMenu.deleteWebpanel();
          }}
        />
      </xul:menupopup>

      <xul:menupopup id="width-size-context">
        <xul:menuitem
          id="setWidthMenu"
          data-l10n-id="sidebar2-keep-width-for-global"
          label="Set width for All Panel"
          accesskey="S"
          onClick={() => {
            bms.keepWidthToGlobalValue();
          }}
        />
      </xul:menupopup>
    </xul:popupset>
  );
}
