export class BMSMouseEvent {
  mouseOver(event) {
    Services.obs.notifyObservers(
      {
        eventType: "mouseOver",
        id: event.target.id,
      },
      "obs-panel",
    );
  }

  mouseOut(event) {
    Services.obs.notifyObservers(
      {
        eventType: "mouseOut",
        id: event.target.id,
      },
      "obs-panel",
    );
  }

  dragStart(event) {
    event.dataTransfer.setData("text/plain", event.target.id);
  }

  dragOver(event) {
    event.preventDefault();
    event.currentTarget.style.borderTop = "2px solid blue";
  }

  dragLeave(event) {
    event.currentTarget.style.borderTop = "";
  }

  drop(event) {
    event.preventDefault();
    const id = event.dataTransfer.getData("text/plain");
    const elm_drag = document.getElementById(id);
    event.currentTarget.parentNode.insertBefore(elm_drag, event.currentTarget);
    event.currentTarget.style.borderTop = "";
    const currentBSD = gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA;
    currentBSD.index.splice(0);
    for (const elem of document.querySelectorAll(".sicon-list")) {
      currentBSD.index.push(
        gBrowserManagerSidebar.getWebpanelObjectById(elem.id),
      );
    }
    Services.prefs.setStringPref(
      `floorp.browser.sidebar2.data`,
      JSON.stringify(currentBSD),
    );
  }
}
