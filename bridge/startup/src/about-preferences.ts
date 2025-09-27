declare global {
  interface Window {
    MozXULElement: MozXULElement;
  }

  namespace Services {
    const wm: {
      getMostRecentWindow(windowType: string): FirefoxWindow;
    };
    const prefs: {
      getStringPref(prefName: string): string;
      setStringPref(prefName: string, value: string): void;
      getBoolPref(prefName: string, defaultValue?: boolean): boolean;
      setBoolPref(prefName: string, value: boolean): void;
    };
    const scriptSecurityManager: {
      getSystemPrincipal(): unknown;
    };
  }

  interface ImportMeta {
    env: {
      MODE: string;
    };
  }
}

interface FirefoxWindow extends Window {
  gBrowser: {
    selectedTab: unknown;
    addTab(url: string, options: unknown): unknown;
  };
}

type MozXULElement = {
  parseXULToFragment(str: string, entities?: Array<string>): DocumentFragment;
};

/**
 * Add Floorp Hub category element
 */
function addFloorpHubCategory(): void {
  const nav_root = document.querySelector("#categories");
  const before_fragment = document.querySelector("#category-sync");

  if (!nav_root || !before_fragment) {
    console.error("Category elements not found");
    return;
  }

  const fragment = window.MozXULElement.parseXULToFragment(`
    <richlistitem
      id="category-nora-link"
      class="category"
      align="center"
      tooltiptext="Floorp Hub"
    >
      <image class="category-icon" src="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyMCAyMCIgd2lkdGg9IjIwIiBoZWlnaHQ9IjIwIiBjbGFzcz0iY2F0ZWdvcnktaWNvbi1zdmciPgogIDxzdHlsZT4KICAgIEBtZWRpYSAocHJlZmVycy1jb2xvci1zY2hlbWU6IGRhcmspIHsKICAgICAgLmljb24tcGF0aCB7IGZpbGw6ICNmZmZmZmY7IH0KICAgIH0KICAgIEBtZWRpYSAocHJlZmVycy1jb2xvci1zY2hlbWU6IGxpZ2h0KSB7CiAgICAgIC5pY29uLXBhdGggeyBmaWxsOiAjMDAwMDAwOyB9CiAgICB9CiAgPC9zdHlsZT4KICA8cGF0aCBjbGFzcz0iaWNvbi1wYXRoIiBkPSJNNi4zMTM3OSAwLjk0Mjg3M0M1LjQ2NzUzIDAuOTQyODczIDQuNDI1OCAxLjU1Mjk0IDQuMDAyNjYgMi4yODU4M0wwLjMxNzM1MSA4LjY4ODI4Qy0wLjEwNTc4NCA5LjQyMTE3LTAuMTA1Nzg0IDEwLjU3ODggMC4zMTczNTEgMTEuMzExN0w0LjAwMjY2IDE3LjcxNDJDNC40MjU4IDE4LjQ0NzEgNS40Njc1MiAxOS4wNTcxIDYuMzEzNzkgMTkuMDU3MUwxMy42ODQ0IDE5LjA1NzFDMTQuNTMwNyAxOS4wNTcxIDE1LjU3MjQgMTguNDQ3MSAxNS45OTU1IDE3LjcxNDJMMTkuNjgwOSAxMS4zMTE3QzIwLjEwNCAxMC41Nzg4IDIwLjEwNCA5LjQyMTE3IDE5LjY4MDkgOC42ODgyOEwxNS45OTU1IDIuMjg1ODNDMTUuNTcyNCAxLjU1Mjk0IDE0LjUzMDcgMC45NDI4NzMgMTMuNjg0NCAwLjk0Mjg3M0w2LjMxMzc5IDAuOTQyODczWk02LjMxMzc5IDIuNDQxOThMMTMuNjg0NCAyLjQ0MTk4QzEzLjk5NTEgMi40NDE5OCAxNC41Mjg1IDIuNzY2MzIgMTQuNjgzOCAzLjAzNTM4TDE4LjM2OTEgOS40Mzc4M0MxOC41MjQ1IDkuNzA2ODkgMTguNTI0NSAxMC4yOTMxIDE4LjM2OTEgMTAuNTYyMkwxNC42ODM4IDE2Ljk2NDZDMTQuNTI4NSAxNy4yMzM3IDEzLjk5NTEgMTcuNTU4IDEzLjY4NDQgMTcuNTU4TDYuMzEzNzkgMTcuNTU4QzYuMDAzMSAxNy41NTggNS40Njk3MyAxNy4yMzM3IDUuMzE0MzkgMTYuOTY0NkwxLjYyOTA3IDEwLjU2MjJDMS40NzM3MyAxMC4yOTMxIDEuNDczNzMgOS43MDY4OSAxLjYyOTA3IDkuNDM3ODNMNS4zMTQzOSAzLjAzNTM4QzUuNDY5NzMgMi43NjYzMiA2LjAwMzEgMi40NDE5OCA2LjMxMzc5IDIuNDQxOThaTTkuMTI0NjMgMy41MDM4NUM4LjUyMzkyIDMuNTAzODUgOC4wMTY5NCAzLjk3MDI2IDcuOTM3ODMgNC41NjU3Mkw3Ljc4MTY3IDUuNzIxMjlMNy40MDY4OSA1LjkzOTkxTDYuMzEzNzkgNS41MDI2N0M1Ljc1ODI3IDUuMjcyNjMgNS4xMTUxIDUuNDgxOTMgNC44MTQ2OCA2LjAwMjM3TDMuOTQwMiA3LjUwMTQ4QzMuNjQwMDQgOC4wMjE0OSAzLjc3NjI4IDguNjk2NjUgNC4yNTI1MiA5LjA2MzA2TDUuMTg5NDYgOS43ODEzOEw1LjE4OTQ2IDEwLjIxODZMNC4yNTI1MiAxMC45MzY5QzMuNzc2MTggMTEuMzAzMyAzLjYzOTk3IDExLjk3ODQgMy45NDAyIDEyLjQ5ODVMNC44MTQ2OCAxMy45OTc2QzUuMTE1MDUgMTQuNTE4IDUuNzU4NDggMTQuNzI2NiA2LjMxMzc5IDE0LjQ5NzNMNy40MDY4OSAxNC4wNjAxTDcuNzgxNjcgMTQuMjc4N0w3LjkzNzgzIDE1LjQzNDNDOC4wMTY5NSAxNi4wMjk4IDguNTIzOTIgMTYuNDk2MSA5LjEyNDYzIDE2LjQ5NjFMMTAuODczNiAxNi40OTYxQzExLjQ3NDMgMTYuNDk2MSAxMS45ODEzIDE2LjAyOTcgMTIuMDYwNCAxNS40MzQzTDEyLjIxNjUgMTQuMjc4N0wxMi41OTEzIDE0LjA2MDFMMTMuNjg0NCAxNC40OTczQzE0LjIzOTggMTQuNzI2NiAxNC44ODMyIDE0LjUxNzkgMTUuMTgzNSAxMy45OTc2TDE2LjA1OCAxMi40OTg1QzE2LjM1ODQgMTEuOTc4MSAxNi4yMjI2IDExLjMwMjkgMTUuNzQ1NyAxMC45MzY5TDE0LjgwODggMTAuMjE4NkwxNC44MDg4IDkuNzgxMzhMMTUuNzQ1NyA5LjA2MzA2QzE2LjIyMjEgOC42OTY2MyAxNi4zNTgyIDguMDIxNTYgMTYuMDU4IDcuNTAxNDhMMTUuMTgzNSA2LjAwMjM3QzE0Ljg4MzIgNS40ODIgMTQuMjM5NyA1LjI3MzM1IDEzLjY4NDQgNS41MDI2N0wxMi41OTEzIDUuOTM5OTFMMTIuMjE2NSA1LjcyMTI5TDEyLjA2MDQgNC41NjU3MkMxMS45ODEzIDMuOTcwMjEgMTEuNDc0MyAzLjUwMzg1IDEwLjg3MzYgMy41MDM4NUMxMC42NTcyIDMuNTAzODUgOS4zNDA5NyAzLjUwMzg1IDkuMTI0NjMgMy41MDM4NVpNOS4zNzQ0OCA1LjAwMjk2QzkuNzE4MjQgNS4wMDI5NiAxMC4yOCA1LjAwMjk2IDEwLjYyMzcgNS4wMDI5NkwxMC43Nzk5IDYuMjgzNDVDMTAuODEwNSA2LjUxNDE2IDEwLjk1MzEgNi42OTgwMiAxMS4xNTQ3IDYuODE0MzlMMTIuMTU0MSA3LjQwNzc5QzEyLjM1NTYgNy41MjQxNiAxMi42MjYxIDcuNTU5MDcgMTIuODQxMiA3LjQ3MDI1TDE0LjAyOCA2Ljk3MDU1TDE0LjYyMTQgOC4wMzI0MkwxMy42MjIgOC44MTMyQzEzLjQzNzUgOC45NTUwOSAxMy4zMDk2IDkuMTczODYgMTMuMzA5NiA5LjQwNjZMMTMuMzA5NiAxMC41OTM0QzEzLjMwOTYgMTAuODI2NCAxMy40MzcxIDExLjA0NDkgMTMuNjIyIDExLjE4NjhMMTQuNjIxNCAxMS45Njc2TDE0LjAyOCAxMy4wMjk1TDEyLjg0MTIgMTIuNTI5N0MxMi42MjYxIDEyLjQ0MDkgMTIuMzU1NiAxMi40NzU4IDEyLjE1NDEgMTIuNTkyMkwxMS4xNTQ3IDEzLjE4NTZDMTAuOTUzMSAxMy4zMDIgMTAuODEwNSAxMy40ODU4IDEwLjc3OTkgMTMuNzE2NUwxMC42MjM3IDE0Ljk5N0w5LjM3NDQ4IDE0Ljk5N0w5LjIxODMyIDEzLjcxNjVDOS4xODc2OSAxMy40ODU4IDkuMDQ1MDkgMTMuMzAyIDguODQzNTQgMTMuMTg1Nkw3Ljg0NDE0IDEyLjU5MjJDNy42NDI1OSAxMi40NzU4IDcuMzcyMTYgMTIuNDQwOSA3LjE1NzA0IDEyLjUyOTdMNS45NzAyNSAxMy4wMjk1TDUuMzc2ODUgMTEuOTY3Nkw2LjM3NjI2IDExLjE4NjhDNi41NjA3NSAxMS4wNDQ5IDYuNjg4NTcgMTAuODI2MSA2LjY4ODU3IDEwLjU5MzRMNi42ODg1NyA5LjQwNjZDNi42ODg1NyA5LjE3MzkzIDYuNTYwNjYgOC45NTUwOSA2LjM3NjI2IDguODEzMkw1LjM3Njg1IDguMDMyNDJMNS45NzAyNSA2Ljk3MDU1TDcuMTU3MDQgNy40NzAyNUM3LjM3MjMgNy41NTk0IDcuNjExMTIgNy41MjQyNiA3LjgxMjkgNy40MDc3OUw4Ljg0MzU0IDYuODE0MzlDOS4wNDUxMyA2LjY5ODAzIDkuMTg3NjggNi41MTQxOSA5LjIxODMyIDYuMjgzNDVMOS4zNzQ0OCA1LjAwMjk2Wk05Ljk5OTExIDguMjUxMDRDOS4wMzMxOCA4LjI1MTA0IDguMjUwMTQgOS4wMzQwNyA4LjI1MDE0IDEwQzguMjUwMTQgMTAuOTY1OSA5LjAzMzE4IDExLjc0OSA5Ljk5OTExIDExLjc0OUMxMC45NjUgMTEuNzQ5IDExLjc0ODEgMTAuOTY1OSAxMS43NDgxIDEwQzExLjc0ODEgOS4wMzQwNyAxMC45NjUgOC4yNTEwNCA5Ljk5OTExIDguMjUxMDRaIi8+Cjwvc3ZnPg==" />
      <label class="category-name" flex="1">
        Floorp Hub
      </label>
    </richlistitem>
  `);

  nav_root.insertBefore(fragment, before_fragment);

  const hubLink = document.querySelector("#category-nora-link");
  if (!hubLink) {
    console.error("Failed to create Floorp Hub element");
    return;
  }

  hubLink.addEventListener("click", () => {
    if (import.meta.env.MODE === "dev") {
      window.location.href = "http://localhost:5183/";
    } else {
      const win = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as FirefoxWindow;
      win.gBrowser.selectedTab = win.gBrowser.addTab("about:hub", {
        relatedToCurrent: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }
  });
}

function hideNewTabPage(): void {
  const pref = Services.prefs.getStringPref("floorp.design.configs");
  const config = JSON.parse(pref).uiCustomization.disableFloorpStart;

  if (!config) {
    document
      .querySelectorAll('#category-home, [data-category="paneHome"]')
      .forEach((el) => {
        el.remove();
      });
  }
}

addFloorpHubCategory();
Services.obs.addObserver(hideNewTabPage, "home-pane-loaded");
