# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestReopenFromLibrary(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestReopenFromLibrary, self).setUp()

        self.original_showForNewBookmarks_pref = self.marionette.get_pref(
            "browser.bookmarks.editDialog.showForNewBookmarks"
        )
        self.original_loadBookmarksInTabs_pref = self.marionette.get_pref(
            "browser.tabs.loadBookmarksInTabs"
        )

        self.marionette.set_pref(
            "browser.bookmarks.editDialog.showForNewBookmarks", False
        )
        self.marionette.set_pref("browser.tabs.loadBookmarksInTabs", True)

    def tearDown(self):
        self.close_all_windows()

        self.marionette.restart(in_app=False, clean=True)

        super(TestReopenFromLibrary, self).tearDown()

    def test_open_bookmark_from_library_with_no_browser_window_open(self):
        bookmark_url = self.marionette.absolute_url("empty.html")
        self.marionette.navigate(bookmark_url)

        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        star_button = self.marionette.find_element("id", "star-button-box")
        star_button.click()

        star_image = self.marionette.find_element("id", "star-button")

        def check(_):
            return "true" in star_image.get_attribute("starred")

        Wait(self.marionette).until(check, message="Failed to star the page")

        win = self.open_chrome_window(
            "chrome://browser/content/places/places.xhtml", False
        )

        self.marionette.close_chrome_window()

        self.marionette.switch_to_window(win)

        # Tree elements can't be accessed in the same way as regular elements,
        # so this uses some code from the places tests and EventUtils.js to
        # select the bookmark in the tree and then double-click it.
        script = """\
          window.PlacesOrganizer.selectLeftPaneContainerByHierarchy(
            PlacesUtils.bookmarks.virtualToolbarGuid
          );

          let node = window.ContentTree.view.view.nodeForTreeIndex(1);
          window.ContentTree.view.selectNode(node);

          // Based on synthesizeDblClickOnSelectedTreeCell
          let tree = window.ContentTree.view;

          if (tree.view.selection.count < 1) {
            throw new Error("The test node should be successfully selected");
          }
          // Get selection rowID.
          let min = {};
          let max = {};
          tree.view.selection.getRangeAt(0, min, max);
          let rowID = min.value;
          tree.ensureRowIsVisible(rowID);
          // Calculate the click coordinates.
          let rect = tree.getCoordsForCellItem(rowID, tree.columns[0], "text");
          let x = rect.x + rect.width / 2;
          let y = rect.y + rect.height / 2;
          let treeBodyRect = tree.body.getBoundingClientRect();
          return [treeBodyRect.left + x, treeBodyRect.top + y]
        """

        position = self.marionette.execute_script(textwrap.dedent(script))
        # These must be integers for pointer_move
        x = round(position[0])
        y = round(position[1])

        self.marionette.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        ).pointer_move(x, y).click().click().perform()

        def window_with_url_open(_):
            urls_in_windows = self.get_urls_for_windows()

            for urls in urls_in_windows:
                if bookmark_url in urls:
                    return True
            return False

        Wait(self.marionette).until(
            window_with_url_open,
            message="Failed to open the browser window from the library",
        )

        # Closes the library window.
        self.marionette.close_chrome_window()

    def get_urls_for_windows(self):
        # There's no guarantee that Marionette will return us an
        # iterator for the opened windows that will match the
        # order within our window list. Instead, we'll convert
        # the list of URLs within each open window to a set of
        # tuples that will allow us to do a direct comparison
        # while allowing the windows to be in any order.
        opened_urls = set()
        for win in self.marionette.chrome_window_handles:
            urls = tuple(self.get_urls_for_window(win))
            opened_urls.add(urls)

        return opened_urls

    def get_urls_for_window(self, win):
        orig_handle = self.marionette.current_chrome_window_handle

        try:
            self.marionette.switch_to_window(win)
            return self.marionette.execute_script(
                """
              if (!window?.gBrowser) {
                return [];
              }
              return window.gBrowser.tabs.map(tab => {
                return tab.linkedBrowser.currentURI.spec;
              });
            """
            )
        finally:
            self.marionette.switch_to_window(orig_handle)
