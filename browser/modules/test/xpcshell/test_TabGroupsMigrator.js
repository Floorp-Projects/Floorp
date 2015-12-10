"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource:///modules/TabGroupsMigrator.jsm");

var gProfD = do_get_profile();

const TEST_STATES = {
  TWO_GROUPS: {
    selectedWindow: 1,
    windows: [
      {
        tabs: [
          {
            entries: [{
              url: "about:robots",
              title: "Robots 1",
            }],
            index: 1,
            hidden: false,
            extData: {
              "tabview-tab": "{\"groupID\":2,\"active\":true}",
            },
          },
          {
            entries: [{
              url: "about:robots",
              title: "Robots 2",
            }],
            index: 1,
            hidden: false,
            extData: {
              "tabview-tab": "{\"groupID\":2}",
            },
          },
          {
            entries: [{
              url: "about:robots",
              title: "Robots 3",
            }],
            index: 1,
            hidden: true,
            extData: {
              "tabview-tab": "{\"groupID\":13}",
            },
          }
        ],
        extData: {
          "tabview-group": "{\"2\":{},\"13\":{\"title\":\"Foopy\"}}",
          "tabview-groups": "{\"nextID\":20,\"activeGroupId\":2,\"totalNumber\":2}",
          "tabview-visibility": "false"
        },
      },
    ]
  },
  NAMED_ACTIVE_GROUP: {
    selectedWindow: 1,
    windows: [
      {
        tabs: [
          {
            entries: [{
              url: "about:mozilla",
              title: "Mozilla 1",
            }],
            index: 1,
            hidden: false,
            extData: {
              "tabview-tab": "{\"groupID\":2,\"active\":true}",
            },
          },
        ],
        extData: {
          "tabview-group": "{\"2\":{\"title\":\"Foopy\"}}",
          "tabview-groups": "{\"nextID\":20,\"activeGroupId\":2,\"totalNumber\":1}",
          "tabview-visibility": "false"
        },
      }
    ],
  },
};

add_task(function* gatherGroupDataTest() {
  let groupInfo = TabGroupsMigrator._gatherGroupData(TEST_STATES.TWO_GROUPS);
  Assert.equal(groupInfo.size, 1, "Information about 1 window");
  let singleWinGroups = [... groupInfo.values()][0];
  Assert.equal(singleWinGroups.size, 2, "2 groups");
  let group2 = singleWinGroups.get("2");
  Assert.ok(!!group2, "group 2 should exist");
  if (group2) {
    Assert.equal(group2.tabs.length, 2, "2 tabs in group 2");
    Assert.equal(group2.tabGroupsMigrationTitle, "", "We don't assign titles to untitled groups");
    Assert.equal(group2.anonGroupID, "1", "We mark an untitled group with an anonymous id");
  }
  let group13 = singleWinGroups.get("13");
  if (group13) {
    Assert.equal(group13.tabs.length, 1, "1 tabs in group 13");
    Assert.equal(group13.tabGroupsMigrationTitle, "Foopy", "Group with title has correct title");
    Assert.ok(!("anonGroupID" in group13), "We don't mark a titled group with an anonymous id");
  }
});

add_task(function* bookmarkingTest() {
  let stateClone = JSON.parse(JSON.stringify(TEST_STATES.TWO_GROUPS));
  let groupInfo = TabGroupsMigrator._gatherGroupData(stateClone);
  let removedGroups = TabGroupsMigrator._removeHiddenTabGroupsFromState(stateClone, groupInfo);
  yield TabGroupsMigrator._bookmarkAllGroupsFromState(groupInfo);
  let bmCounter = 0;
  let bmParents = {};
  let bookmarks = [];
  let onResult = bm => {
    bmCounter++;
    bmParents[bm.parentGuid] = (bmParents[bm.parentGuid] || 0) + 1;
    Assert.ok(bm.title.startsWith("Robots "), "Bookmark title(" + bm.title + ")  should start with 'Robots '");
  };
  yield PlacesUtils.bookmarks.fetch({url: "about:robots"}, onResult);
  Assert.equal(bmCounter, 3, "Should have seen 3 bookmarks");
  Assert.equal(Object.keys(bmParents).length, 2, "Should be in 2 folders");

  let ancestorGuid;
  let parents = Object.keys(bmParents).map(guid => {
    PlacesUtils.bookmarks.fetch({guid}, bm => {
      ancestorGuid = bm.parentGuid;
      if (bmParents[bm.guid] == 1) {
        Assert.equal(bm.title, "Foopy", "Group with 1 kid has right title");
      } else {
        Assert.ok(bm.title.includes("1"), "Group with more kids should have anon ID in title (" + bm.title + ")");
      }
    });
  });
  yield Promise.all(parents);

  yield PlacesUtils.bookmarks.fetch({guid: ancestorGuid}, bm => {
    Assert.equal(bm.title,
      gBrowserBundle.GetStringFromName("tabgroups.migration.tabGroupBookmarkFolderName"),
      "Should have the right title");
  });
});

add_task(function* bookmarkNamedActiveGroup() {
  let stateClone = JSON.parse(JSON.stringify(TEST_STATES.NAMED_ACTIVE_GROUP));
  let groupInfo = TabGroupsMigrator._gatherGroupData(stateClone);
  let removedGroups = TabGroupsMigrator._removeHiddenTabGroupsFromState(stateClone, groupInfo);
  yield TabGroupsMigrator._bookmarkAllGroupsFromState(groupInfo);
  let bmParents = {};
  let bmCounter = 0;
  let onResult = bm => {
    bmCounter++;
    bmParents[bm.parentGuid] = (bmParents[bm.parentGuid] || 0) + 1;
    Assert.ok(bm.title.startsWith("Mozilla "), "Bookmark title (" + bm.title + ")  should start with 'Mozilla '");
  };
  yield PlacesUtils.bookmarks.fetch({url: "about:mozilla"}, onResult);
  Assert.equal(bmCounter, 1, "Should have seen 1 bookmarks");
  let parentPromise = PlacesUtils.bookmarks.fetch({guid: Object.keys(bmParents)[0]}, bm => {
    Assert.equal(bm.title, "Foopy", "Group with 1 kid has right title");
  });
  yield parentPromise;
});

add_task(function* removingTabGroupsFromJSONTest() {
  let stateClone = JSON.parse(JSON.stringify(TEST_STATES.TWO_GROUPS));
  let groupInfo = TabGroupsMigrator._gatherGroupData(stateClone);
  let removedGroups = TabGroupsMigrator._removeHiddenTabGroupsFromState(stateClone, groupInfo);
  Assert.equal(removedGroups.windows.length, 1, "Removed 1 group which looks like a window in removed data");
  Assert.equal(removedGroups.windows[0].tabs.length, 1, "Removed group had 1 tab");
  Assert.ok(!stateClone.windows[0].extData, "extData removed from window");
  stateClone.windows[0].tabs.forEach(tab => {
    Assert.ok(!tab.extData, "extData removed from tab");
  });
  Assert.ok(stateClone.windows[0].tabs.length, 2, "Only 2 tabs remain in the window");
});

add_task(function* backupTest() {
  yield TabGroupsMigrator._createBackup(JSON.stringify(TEST_STATES.TWO_GROUPS));
  let f = Services.dirsvc.get("ProfD", Components.interfaces.nsIFile);
  f.append("tabgroups-session-backup.json");
  ok(f.exists(), "Should have created the file");

  let txt = (new TextDecoder()).decode(yield OS.File.read(f.path));
  Assert.deepEqual(JSON.parse(txt), TEST_STATES.TWO_GROUPS, "Should have written the expected state.");

  f.remove(false);
});

add_task(function* migrationPageDataTest() {
  let stateClone = JSON.parse(JSON.stringify(TEST_STATES.TWO_GROUPS));
  let groupInfo = TabGroupsMigrator._gatherGroupData(stateClone);
  let removedGroups = TabGroupsMigrator._removeHiddenTabGroupsFromState(stateClone, groupInfo);
  TabGroupsMigrator._createBackgroundTabGroupRestorationPage(stateClone, removedGroups);
  Assert.equal(stateClone.windows.length, 1, "Should still only have 1 window");
  Assert.equal(stateClone.windows[0].tabs.length, 3, "Should now have 3 tabs");

  let url = "chrome://browser/content/aboutTabGroupsMigration.xhtml";
  let formdata = {id: {sessionData: JSON.stringify(removedGroups)}, url};
  Assert.deepEqual(stateClone.windows[0].tabs[2],
    {
      entries: [{url}],
      formdata,
      index: 1
    },
    "Should have added expected tab at the end of the tab list.");
});
