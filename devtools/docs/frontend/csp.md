
The DevTools toolbox is loaded in an iframe pointing to about:devtools-toolbox. This iframe has a [Content Security Policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP) (CSP) applied, which will mitigate potential attacks. However this may limit the resources that can be loaded in the toolbox documenth.

# Current DevTools CSP

The current policy for about:devtools-toolbox is:
```
default-src chrome: resource:; img-src chrome: resource: data:;
```

This means:
- `chrome://` and `resource://` are allowed for any resource
- `chrome://` and `resource://` and `data://` are allowed for images

For more information about which resources and requests are in scope of the CSP, you can read the [default-src documentation on MDN](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/default-src).

# Scope of the DevTools CSP

This content security policy only applies to the toolbox document for now. If you are working within the document of a panel or if you are working on the server, those limitations should not apply.

Note that even when working in the document of a panel, we are sometimes interacting with the toolbox document, for instance to show tooltips. So typically any resource created for a tooltip will be subject to the CSP limitations.

# Recognizing CSP issues

Open the Browser Toolbox, if you see errors such as

```
JavaScript Error: "Content Security Policy: The page’s settings blocked the loading of a resource [...]"
```

it means you are trying to load a resource with a forbidden scheme.

# Fixing CSP issues

If your implementation hits a CSP issue, the first suggestion is to try to use a supported scheme. If this is not an option, check if you can perform your request or load your resource outside of the toolbox document. For instance if the resource you are loading is related to the debugged target, the request can (and probably should) be made from an actor in the DevTools server and then forwarded from the server to the client. Requests made by the server will not be impacted by the CSP.

If it seems like the only solution is to update the CSP, get in touch with security peers in order to discuss about your use case. You can [file a bug in Core/DOM: security](https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=DOM%3A%20Security).

# Fixing CSP issues in tests

If the issue comes from test code, it should be possible to update the test to use a supported scheme. A typical issue might be trying to load an iframe inside of the toolbox with a data-uri. Instead, you can create an HTML support file and load it from either a chrome:// or a resource:// URL.

In general once a support file is added you can access it via:
- `https://example.com/browser/[path_to_file]`
- or `chrome://mochitests/content/browser/[path_to_file]`

For instance [devtools/client/aboutdebugging/test/browser/resources/service-workers/controlled-sw.html](https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser/resources/service-workers/controlled-sw.html) is accessed in tests via `http://example.com/browser/devtools/client/aboutdebugging/test/browser/resources/service-workers/controlled-sw.html`.

If you absolutely have to use an unsupported scheme, you can turn off CSPs for the test only. To do so, you need to temporarily update two preferences:

```
await pushPref("security.csp.enable", false);
await pushPref("csp.skip_about_page_has_csp_assert", true);
```

The `pushPref` helper will ensure the preferences come back to their initial value at the end of the test.
