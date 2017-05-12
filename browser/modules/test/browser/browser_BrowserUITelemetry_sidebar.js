// Test the sidebar counters in BrowserUITelemetry.
"use strict";

const { BrowserUITelemetry: BUIT } = Cu.import("resource:///modules/BrowserUITelemetry.jsm", {});

add_task(async function testSidebarOpenClose() {
  // Reset BrowserUITelemetry's world.
  BUIT._countableEvents = {};

  await SidebarUI.show("viewTabsSidebar");

  let counts = BUIT._countableEvents[BUIT.currentBucket];

  Assert.deepEqual(counts, { sidebar: { viewTabsSidebar: { show: 1 } } });
  await SidebarUI.hide();
  Assert.deepEqual(counts, { sidebar: { viewTabsSidebar: { show: 1, hide: 1 } } });

  await SidebarUI.show("viewBookmarksSidebar");
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 1, hide: 1 },
      viewBookmarksSidebar: { show: 1 },
    }
  });
  // Re-open the tabs sidebar while bookmarks is open - bookmarks should
  // record a close.
  await SidebarUI.show("viewTabsSidebar");
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 2, hide: 1 },
      viewBookmarksSidebar: { show: 1, hide: 1 },
    }
  });
  await SidebarUI.hide();
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 2, hide: 2 },
      viewBookmarksSidebar: { show: 1, hide: 1 },
    }
  });
  // Toggle - this will re-open viewTabsSidebar
  await SidebarUI.toggle("viewTabsSidebar");
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 3, hide: 2 },
      viewBookmarksSidebar: { show: 1, hide: 1 },
    }
  });
  await SidebarUI.toggle("viewTabsSidebar");
  Assert.deepEqual(counts, {
    sidebar: {
      viewTabsSidebar: { show: 3, hide: 3 },
      viewBookmarksSidebar: { show: 1, hide: 1 },
    }
  });
});
