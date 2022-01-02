===========
JSON viewer
===========

The JSON viewer is new in Firefox 44.

Before Firefox 53, the JSON viewer is enabled by default only in Firefox Developer Edition and Firefox Nightly. To enable this feature in other release channels, set the ```devtools.jsonview.enabled``` preference to ```true```.

From Firefox 53 onwards, the JSON viewer is also enabled by default in Beta and the normal release version of Firefox.


Firefox includes a JSON viewer. If you open a JSON file in the browser, or view a remote URL with the Content-Type set to application/json, it is parsed and given syntax highlighting. Arrays and objects are shown collapsed, and you can expand them using the "+" icons.

The JSON viewer provides a search box that you can use to filter the JSON.

You can also view the raw JSON and pretty-print it.

Finally, if the document was the result of a network request, the viewer displays the request and response headers.
