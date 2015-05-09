=============================================
Directory Links Architecture and Data Formats
=============================================

Directory links are enhancements to the new tab experience that combine content
Firefox already knows about from user browsing with external content. There are
3 kinds of links:

- directory links fill in additional tiles on the new tab page if there would
  have been empty tiles because the user has a clean profile or cleared history
- suggested links are shown if certain triggering criteria matches the user's
  browsing behavior, i.e., if the user has a top site that matches one of
  several possible sites. E.g., only show a sports suggestion if the user has a
  sport site as a top site
- enhanced links replace a matching user's visible history tile from the same
  site but only the visual aspects: title, image, and rollover image

To power the above features, DirectoryLinksProvider module downloads, at most
once per 24 hours, the directory source links as JSON with enough data for
Firefox to determine what should be shown or not. This module also handles
reporting back data about the tiles via asynchronous pings that don't return
data from the server.

For the directory source and ping endpoints, the default preference values point
to Mozilla key-pinned servers with encryption. No cookies are set by the servers
but not enforced by Firefox.

- default directory source endpoint:
  https://tiles.services.mozilla.com/v3/links/fetch/%LOCALE%/%CHANNEL%
- default directory ping endpoint: https://tiles.services.mozilla.com/v3/links/


Preferences
===========

There are two main preferences that control downloading links and reporting
metrics.

``browser.newtabpage.directory.source``
---------------------------------------

This endpoint tells Firefox where to download directory source file as a GET
request. It should return JSON of the appropriate format containing the relevant
links data. The value can be a data URI, e.g., an empty JSON object effectively
turns off remote downloading: ``data:text/plain,{}``

The preference value will have %LOCALE% and %CHANNEL% replaced by the
appropriate values for the build of Firefox, e.g.,

- directory source endpoint:
  https://tiles.services.mozilla.com/v3/links/fetch/en-US/release

``browser.newtabpage.directory.ping``
-------------------------------------

This endpoint tells Firefox where to report Tiles metrics as a POST request. The
data is sent as a JSON blob. Setting it to empty effectively turns off reporting
of Tiles data.

A path segment will be appended to the endpoint of "view" or "click" depending
on the type of ping, e.g.,

- ``view`` ping endpoint: https://tiles.services.mozilla.com/v3/links/view
- ``click`` ping endpoint: https://tiles.services.mozilla.com/v3/links/click


Data Flow
=========

When Firefox starts, it checks for a cached directory source file. If one
exists, it checks for its timestamp to determine if a new file should be
downloaded.

If a directory source file needs to be downloaded, a GET request is made then
cacheed and unpacked the JSON into the different types of links. Various checks
filter out invalid links, e.g., those with http-hosted images or those that
don't fit the allowed suggestions.

When a new tab page is built, DirectoryLinksProvider module provides additional
link data that is combined with history link data to determine which links can
be displayed or not.

When a new tab page is shown, a ``view`` ping is sent with relevant tiles data.
Similarly, when the user clicks on various parts of tiles (to load the page,
pin, block, etc.), a ``click`` ping is sent with similar data. Both of these can
trigger downloading of fresh directory source links if 24 hours have elapsed
since last download.

Users can turn off the ping with in-new-tab-page controls.

As the new tab page is rendered, any images for tiles are downloaded if not
already cached. The default servers hosting the images are Mozilla CDN that
don't use cookies: https://tiles.cdn.mozilla.net/


Source JSON Format
==================

Firefox expects links data in a JSON object with top level keys each providing
an array of tile objects. The keys correspond to the different types of links:
``directory``, ``suggested``, and ``enhanced``.

Example
-------

Below is an example directory source file::

  {
      "directory": [
          {
              "bgColor": "",
              "directoryId": 498,
              "enhancedImageURI": "https://tiles.cdn.mozilla.net/images/d11ba0b3095bb19d8092cd29be9cbb9e197671ea.28088.png",
              "imageURI": "https://tiles.cdn.mozilla.net/images/1332a68badf11e3f7f69bf7364e79c0a7e2753bc.5316.png",
              "title": "Mozilla Community",
              "type": "affiliate",
              "url": "http://contribute.mozilla.org/"
          }
      ],
      "enhanced": [
          {
              "bgColor": "",
              "directoryId": 776,
              "enhancedImageURI": "https://tiles.cdn.mozilla.net/images/44a14fc405cebc299ead86514dff0e3735c8cf65.10814.png",
              "imageURI": "https://tiles.cdn.mozilla.net/images/20e24aa2219ec7542cc8cf0fd79f0c81e16ebeac.11859.png",
              "title": "TurboTax",
              "type": "sponsored",
              "url": "https://turbotax.intuit.com/"
          }
      ],
      "suggested": [
          {
              "bgColor": "#cae1f4",
              "directoryId": 702,
              "frecent_sites": [
                  "addons.mozilla.org",
                  "air.mozilla.org",
                  "blog.mozilla.org",
                  "bugzilla.mozilla.org",
                  "developer.mozilla.org",
                  "etherpad.mozilla.org",
                  "hacks.mozilla.org",
                  "hg.mozilla.org",
                  "mozilla.org",
                  "planet.mozilla.org",
                  "quality.mozilla.org",
                  "support.mozilla.org",
                  "treeherder.mozilla.org",
                  "wiki.mozilla.org"
              ],
              "frequency_caps": {"daily": 3, "total": 10},
              "imageURI": "https://tiles.cdn.mozilla.net/images/9ee2b265678f2775de2e4bf680df600b502e6038.3875.png",
              "time_limits": {"start": "2014-01-01T00:00:00.000Z", "end": "2014-02-01T00:00:00.000Z"},
              "title": "Thanks for testing!",
              "type": "affiliate",
              "url": "https://www.mozilla.com/firefox/tiles"
          }
      ]
  }

Link Object
-----------

Each link object has various values that Firefox uses to display a tile:

- ``url`` - string url for the page to be loaded when the tile is clicked. Only
  https and http URLs are allowed.
- ``title`` - string that appears below the tile.
- ``type`` - string relationship of the link to Mozilla. Expected values:
  affiliate, organic, sponsored.
- ``imageURI`` - string url for the tile image to show. Only https and data URIs
  are allowed.
- ``enhancedImageURI`` - string url for the image to be shown before the user
  hovers. Only https and data URIs are allowed.
- ``bgColor`` - string css color for additional fill background color.
- ``directoryId`` - id of the tile to be used during ping reporting

Suggested Link Object Extras
----------------------------

A suggested link has additional values:

- ``frecent_sites`` - array of strings of the sites that can trigger showing a
  Suggested Tile if the user has the site in one of the top 100 most-frecent
  pages. Only preapproved array of strings that are hardcoded into the
  DirectoryLinksProvider module are allowed.
- ``frequency_caps`` - an object consisting of daily and total frequency caps
  that limit the number of times a Suggested Tile can be shown in the new tab
  per day and overall.
- ``time_limits`` - an object consisting of start and end timestamps specifying
  when a Suggested Tile may start and has to stop showing in the newtab.
  The timestamp is expected in ISO_8601 format: '2014-01-10T20:00:00.000Z'

The preapproved arrays follow a policy for determining what topic grouping is
allowed as well as the composition of a grouping. The topics are broad
uncontroversial categories, e.g., Mobile Phone, News, Technology, Video Game,
Web Development. There are at least 5 sites within a grouping, and as many
popular sites relevant to the topic are included to avoid having one site be
clearly dominant. These requirements provide some deniability of which site
actually triggered a suggestion during ping reporting, so it's more difficult to
determine if a user has gone to a specific site.


Ping JSON Format
================

Firefox reports back an action and the state of tiles on the new tab page based
on the user opening a new tab or clicking a tile. The top level keys of the
ping:

- ``locale`` - string locale of the Firefox build
- ``tiles`` - array of tiles ping objects

An additional key at the top level indicates which action triggered the ping.
The value associated to the action key is the 0-based index into the tiles array
of which tile triggered the action. Valid actions: block, click, pin, sponsored,
sponsored_link, unpin, view. E.g., if the second tile is being clicked, the ping
will have ``"click": 1``

Example
-------

Below is an example ``click`` ping with 3 tiles: a pinned suggested tile
followed by a history tile and a directory tile. The first tile is being
blocked::

  {
      "locale": "en-US",
      "tiles": [
          {
              "id": 702,
              "pin": 1,
          },
          {},
          {
              "id": 498,
          }
      ],
      "block": 0
  }

Tiles Ping Object
-----------------

Each tile of the new tab page is reported back as part of the ping with some or
none of the following optional values:

- ``id`` - id that was provided as part of the downloaded link object (for all
  types of links: directory, suggested, enhanced); not present if the tile was
  created from user behavior, e.g., visiting pages
- ``pinned`` - 1 if the tile is pinned; not present otherwise
- ``pos`` - integer position if the tile is not in the natural order, e.g., a
  pinned tile after an empty slot; not present otherwise
- ``score`` - integer truncated score based on the tile's frecency; not present
  if 0
- ``url`` - empty string if it's an enhanced tile; not present otherwise
