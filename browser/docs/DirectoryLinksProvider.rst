=============================================
Directory Links Architecture and Data Formats
=============================================

Directory links are enhancements to the new tab experience that combine content
Firefox already knows about from user browsing with external content. There is 1
kind of links:

- directory links fill in additional tiles on the new tab page if there would
  have been empty tiles because the user has a clean profile or cleared history

To power the above features, DirectoryLinksProvider module downloads, at most
once per 24 hours, the directory source links as JSON with enough data for
Firefox to determine what should be shown or not.

For the directory source endpoint, the default preference values point
to Mozilla key-pinned servers with encryption. No cookies are set by the servers
and Firefox enforces this by making anonymous requests.

- default directory source endpoint:
  https://tiles.services.mozilla.com/v3/links/fetch/%LOCALE%/%CHANNEL%


Preferences
===========

There is one main preference that controls downloading links.

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


Data Flow
=========

When Firefox starts, it checks for a cached directory source file. If one
exists, it checks for its timestamp to determine if a new file should be
downloaded.

If a directory source file needs to be downloaded, a GET request is made then
cached and unpacked the JSON into the different types of links. Various checks
filter out invalid links, e.g., those with http-hosted images or those that
don't fit the allowed suggestions.

When a new tab page is built, DirectoryLinksProvider module provides additional
link data that is combined with history link data to determine which links can
be displayed or not.

As the new tab page is rendered, any images for tiles are downloaded if not
already cached. The default servers hosting the images are Mozilla CDN that
don't use cookies: https://tiles.cdn.mozilla.net/ and Firefox enforces that the
images come from mozilla.net or data URIs when using the default directory
source.


Source JSON Format
==================

Firefox expects links data in a JSON object with a top level "directory" key
providing an array of tile objects.

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
      ]
  }

Link Object
-----------

Each link object has various values that Firefox uses to display a tile:

- ``url`` - string url for the page to be loaded when the tile is clicked. Only
  https and http URLs are allowed.
- ``title`` - string that appears below the tile.
- ``type`` - string relationship of the link to Mozilla. Expected values:
  affiliate.
- ``imageURI`` - string url for the tile image to show. Only https and data URIs
  are allowed.
- ``enhancedImageURI`` - string url for the image to be shown before the user
  hovers. Only https and data URIs are allowed.
- ``bgColor`` - string css color for additional fill background color.
- ``directoryId`` - id of the tile to be used during ping reporting
