'use strict'

var wm = Cc['@mozilla.org/appshell/window-mediator;1'].getService(Ci.nsIWindowMediator)

var listener = {
  onOpenWindow(xulWin) {
    let win = xulWin.docShell.domWindow
    win.addEventListener('load', () => {
      if (win.document.documentElement.getAttribute('windowtype') != 'navigator:browser') {
        return
      }
      patch(win)
    }, { once: true })
  }
}

function load() {
  for (let win of wm.getEnumerator('navigator:browser')) {
    patch(win)
  }
  wm.addListener(listener)
}

function unload() {
  for (let win of wm.getEnumerator('navigator:browser')) {
    unpatch(win)
  }
  wm.removeListener(listener)
}

function patch(win) {
  try {
    var tabsProto = win.customElements.get('tabbrowser-tabs').prototype
  } catch(e) {
    console.warn('Paxmod: tab box not found in this win')
    return
  }
  if (tabsProto._positionPinnedTabs_orig) {
    console.warn('Paxmod: tab box already patched in this win')
    return
  }
  function dropIndex(tabs, event) {
    for (let tab of tabs) {
      let rect = tab.getBoundingClientRect()
      if (event.screenY >= tab.screenY &&
          event.screenY < (tab.screenY + rect.height) &&
          event.screenX < (tab.screenX + (rect.width / 2))) {
        // First tab right of the cursor, and in the cursor's row
        return tabs.indexOf(tab)
      }
      if (event.screenY <= tab.screenY) {
        // Entered a new tab row
        return tabs.indexOf(tab)
      }
    }
  }
  function patchMethod(name, f) {
    tabsProto[name + '_orig'] = tabsProto[name]
    tabsProto[name] = f
  }
  patchMethod('_positionPinnedTabs', function() {
    this._positionPinnedTabs_orig()
    // Remove visual offset of pinned tabs
    this.style.paddingInlineStart = ''
    for (let tab of this.allTabs) {
      tab.style.marginInlineStart = ''
    }
  })
  patchMethod('on_drop', function(event) {
    let dt = event.dataTransfer
    if (dt.dropEffect !== 'move') {
      return this.on_drop_orig(event)
    }
    let draggedTab = dt.mozGetDataAt('application/x-moz-tabbrowser-tab', 0)
    draggedTab._dragData.animDropIndex = dropIndex(this.allTabs, event)
    return this.on_drop_orig(event)
  })
  patchMethod('on_dragover', function(event) {
    let dt = event.dataTransfer
    if (dt.dropEffect !== 'move') {
      return this.on_dragover_orig(event)
    }
    let draggedTab = dt.mozGetDataAt('application/x-moz-tabbrowser-tab', 0)
    let movingTabs = draggedTab._dragData.movingTabs
    draggedTab._dragData.animDropIndex = dropIndex(this.allTabs, event)
    this.on_dragover_orig(event)
    // Reset rules that visualize dragging because they don't work in multi-row
    for (let tab of this.allTabs) {
      tab.style.transform = ''
    }
  })
  patchMethod('_handleTabSelect', function(aInstant) {
    // Only when "overflow" attribute is set, the selected tab will get
    // automatically scrolled into view
    this.setAttribute('overflow', 'true')
    this._handleTabSelect_orig(aInstant)
  })
  win.document.querySelector('#tabbrowser-tabs')._positionPinnedTabs()
  // Make sure the arrowscrollbox doesn't swallow mouse wheel events, so they
  // get propagated to the tab list, allowing the user to scroll up and down
  let arrowscrollbox = win.document.querySelector('#tabbrowser-arrowscrollbox')
  if (arrowscrollbox) {
    arrowscrollbox.removeEventListener('wheel', arrowscrollbox.on_wheel)
  } else {
    console.warn('Paxmod: arrowscrollbox not found')
  }
  let TabsToolbar = win.document.querySelector('#TabsToolbar')
  if (TabsToolbar) {
    TabsToolbar.setAttribute('multibar', 'true');
  } else {
    console.warn('Paxmod: TabsToolbar not found')
  }
}

function unpatch(win) {
  var tabsProto = win.customElements.get('tabbrowser-tabs').prototype
  if (!tabsProto._positionPinnedTabs_orig) {
    console.warn('Paxmod: tab box not patched')
    return
  }
  function unpatchMethod(name) {
    tabsProto[name] = tabsProto[name + '_orig']
    delete tabsProto[name + '_orig']
  }
  unpatchMethod('_positionPinnedTabs')
  unpatchMethod('on_drop')
  unpatchMethod('on_dragover')
  unpatchMethod('_handleTabSelect')
  let arrowscrollbox = win.document.querySelector('#tabbrowser-arrowscrollbox')
  if (arrowscrollbox) {
    arrowscrollbox.addEventListener('wheel', arrowscrollbox.on_wheel)
  } else {
    console.warn('Paxmod: arrowscrollbox not found')
  }
  let TabsToolbar = win.document.querySelector('#TabsToolbar')
  if (TabsToolbar) {
    TabsToolbar.removeAttribute('multibar');
  } else {
    console.warn('Paxmod: TabsToolbar not found')
  }
}

this.paxmod = class extends ExtensionAPI {
  onShutdown(reason) {
    unload()
  }

  getAPI(context) {
    return {
      paxmod: {
        async load() {
          load()
        }
      }
    }
  }
}
