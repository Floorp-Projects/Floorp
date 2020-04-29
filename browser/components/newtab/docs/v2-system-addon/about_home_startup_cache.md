# The `about:home` startup cache

By default, a user's browser session starts with a single window and a single tab, pointed at about:home. This means that it's important to ensure that `about:home` loads as quickly as possible to provide a fast overall startup experience.

`about:home`, which is functionally identical to `about:newtab`, is generated dynamically by calculating an appropriate state object in the parent process, and passing it down to a content process into the React library in order to render the final interactive page. This is problematic during the startup sequence, as calculating that initial state can be computationally expensive, and requires multiple reads from the disk.

The `about:home` startup cache is an attempt to address this expense. It works by assuming that between browser sessions, `about:home` _usually_ doesn't need to change.

## Components of the `about:home` startup cache mechanism

There are 3 primary components to the cache mechanism:

### The HTTP Cache

The HTTP cache is normally used for caching webpages retrieved over the network, but seemed like the right fit for storage of the `about:home` cache as well.

The HTTP cache is usually queried by the networking stack when browsing the web. The HTTP cache is, however, not typically queried when accessing `chrome://` or `resource://` URLs, so we have to do it ourselves, manually for the `about:home` case. This means giving `about:home` special capabilities for populating and reading from the HTTP cache. In order to avoid potential security issues, this requires that we sequester `about:home` / `about:newtab` in their own special content process. The "privileged about content process" exists for this purpose, and is also used for `about:logins` and `about:certificate`.

The HTTP cache lives in the parent process, and so any read and write operations need to be initiated in the parent process. Thankfully, however, the HTTP cache accepts data using `nsIOutputStream` and serves it using `nsIInputStream`. We can send `nsIInputStream` over the message manager, and convert an `nsIInputStream` into an `nsIOutputStream`, so we have everything we need to efficiently communicate with the "privileged about content process" to save and retrieve page data.

The official documentation for the HTTP cache [can be found here](https://developer.mozilla.org/en-US/docs/Mozilla/HTTP_cache).

### `AboutHomeStartupCache`

This singleton component lives inside of `BrowserGlue` to avoid having to load yet another JSM out of the `omni.ja` file in the parent process during startup.

`AboutHomeStartupCache` is responsible for feeding the "privileged about content process" with the `nsIInputStream`'s that it needs to present the initial `about:home` document. It is also responsible for populating the cache with updated versions of `about:home` that are sent by the "privileged about content process".

Since accessing the HTTP cache is asynchronous, there is an opportunity for a race, where the cache can either be accessed and available before the initial `about:home` is requested, or after. To accommodate for both cases, the `AboutHomeStartupCache` constructs `nsIPipe` instances, which it sends down to the "privileged about content process" as soon as one launches.

If the HTTP cache entry is already available when the process launches, and cached data is available, we connect the cache to the `nsIPipe`'s to stream the data down to the "privileged about content process".

If the HTTP cache is not yet available, we hold references to those `nsIPipe` instances, and wait until the cache entry is available. Only then do we connect the cache entry to the `nsIPipe` instances to send the data down to the "privileged about content process".

### `AboutNewTabService`

The `AboutNewTabService` is used by the `AboutRedirector` in both the parent and content processes to determine how to handle attempts to load `about:home` and `about:newtab`.

There are distinct versions of the `AboutNewTabService` - one for the parent process (`BaseAboutNewTabService`), and one for content processes (`AboutNewTabChildService`, which inherits from `BaseAboutNewTabService`).

The `AboutRedirector`, when running inside of a "privileged about content process" knows to direct attempts to load `about:home` to `AboutNewTabChildService`'s `aboutHomeCacheChannel` method. This method is then responsible for choosing whether or not to return an `nsIChannel` for the cached document, or for the dynamically generated version of `about:home`.

### `AboutHomeStartupCacheChild`

This singleton component lives inside of the "privileged about content process", and is initialized as soon as the message is received from the parent that includes the `nsIInputStream`'s that will be used to potentially load from the cache.

When the `AboutRedirector` in the "privileged about content process" notices that a request has been made to `about:home`, it asks `nsIAboutNewTabService` to return a new `nsIChannel` for that document. The `AboutNewTabChildService` then checks to see if the `AboutHomeStartupCacheChild` can return an `nsIChannel` for any cached content.

If, at this point, nothing has been streamed from the parent, we fall back to loading the dynamic `about:home` document. This might occur if the cache doesn't exist yet, or if we were too slow to pull it off of the disk.

The `AboutHomeStartupCacheChild` will also be responsible for generating the cache periodically. Periodically, the `AboutNewTabService` will send down the most up-to-date state for `about:home` from the parent process, and then the `AboutHomeStartupCacheChild` will generate document markup using ReactDOMServer within a `ChromeWorker`. After that's generated, the "privileged about content process" will send up `nsIInputStream` instances for both the markup and the script for the initial page state. The `AboutHomeStartupCache` singleton inside of `BrowserGlue` is responsible for receiving those `nsIInputStream`'s and persisting them in the HTTP cache for the next start.

## What is cached?

Two things are cached:

1. The raw HTML mark-up of `about:home`.
2. A small chunk of JavaScript that "hydrates" the markup through the React libraries, allowing the page to become interactive after painting.

The JavaScript being cached cannot be put directly into the HTML mark-up as inline script due to the CSP of `about:home`, which does not allow inline scripting. Instead, we load a script from `about:home?jscache`. This goes through the same mechanism for retrieving the HTML document from the cache, but instead pulls down the cached script.

If the HTML mark-up is cached, then we presume that the script is also cached. We cannot cache one and not the other. If only one cache exists, or only one has been sent down to the "privileged about content process" by the time the `about:home` document is requested, then we fallback to loading the dynamic `about:home` document.

## Refreshing the cache

The cache is refreshed periodically by having `ActivityStreamMessageChannel` tell `AboutHomeStartupCache` when it has sent any messages down to the preloaded `about:newtab`. In general, such messages are a good hint that something visual has updated for the next `about:newtab`, and that the cache should probably be refreshed.

`AboutHomeStartupCache` debounces notifications about such messages, since they tend to be bursty.

## Invalidating the cache

It's possible that the composition or layout of `about:home` will change over time from release to release. When this occurs, it might be desirable to invalidate any pre-existing cache that might exist for a user, so that they don't see an outdated `about:home` on startup.

To do this, we set a version number on the cache entry, and ensure that the version number is equal to our expectations on startup. If the version number does not match our expectation, then the cache is discarded and the `about:home` document will be rendered dynamically.

The version number is statically defined inside of `AboutHomeStartupCache`. Simply increase that number to invalidate caches with lesser version numbers.

## Handling errors

`about:home` is typically the first thing that the user sees upon starting the browser. It is critically important that it function quickly and correctly. If anything happens to go wrong when retrieving or saving to the cache, we should fall back to generating the document dynamically.

As an example, it's theoretically possible for the browser to crash while in the midst of saving to the cache. In that case, we might have a partial document saved, or a partial script saved - neither of which is acceptable.

Thankfully, the HTTP cache was designed with resilience in mind, so partially written entries are automatically discarded, which allows us to fall back to the dynamic page generation mode.

As additional redundancy to that resilience, we also make sure to create a new nsICacheEntry every time the cache is populated, and write the version metadata as the last step. Since the version metadata is written last, we know that if it's missing when we try to load the cache that the writing of the page and the script did not complete, and that we should fall back to dynamically rendering the page.
