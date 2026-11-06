# Profile feed service

The parent-process service owns one SQLite connection and one scheduler for the
normal profile. The New Tab gateway must reject private-window calls before
starting it. No subscriptions or sample articles are installed by default.

- RSS 2.0 / Atom 1.0; HTML `rel=alternate` discovery returns choices, without
  fetching all candidates. Atom `content src` is never fetched.
- Only feed-delivered content is stored. HTML is converted to a closed AST;
  source HTML is never inserted into the privileged reader. Image placeholders
  require an explicit request for an article-owned image ID.
- Requests use an ordinary content principal for each validated HTTP hop,
  anonymous/cookie-omit channels, browser proxy and certificate policy, at most
  5 redirects and one 20-second deadline across the chain. Native redirects are
  stopped and reopened with the next URL’s content principal, without lending
  chrome privileges or retaining an opaque cross-origin response. Conditional
  validators are dropped across origins.
- The request pool permits 4 concurrent jobs and at most 2 for the origin of
  each request's starting URL. Discovery and explicit image requests share this
  pool. Different starting origins that redirect to one origin can share up to
  the global 4 slots; the limit is not counted separately for redirect hops.
  Same-feed updates coalesce. Manual retry can bypass local backoff but respects
  a persisted server Retry-After deadline.
- Feed bodies are bounded to 5 MiB after channel decoding. Images are at most 8
  MiB and must decode as PNG/JPEG/GIF/WebP/AVIF; SVG and HTML are rejected.
- The initial cache policy is 90 days and 500 non-starred articles per feed.
  Stars are excluded. Unsubscribing stops requests, removes non-starred content,
  and keeps stars with the disabled subscription's stable identity.
- SQLite has a 100 MiB physical page budget. Read-state tombstones are bounded
  to 100,000 entries / 365 days, so state history is intentionally finite. Old
  content beyond that history may be newly recognized if republished.
- Schema version 2 adds that Retry-After deadline to version 1 in a transaction.
  Unknown database versions or corruption fail without deleting/replacing the
  database. Transactions preserve prior successful content and read/star state.
- `AsyncShutdown.profileBeforeChange` waits for fetch cancellation and store
  closure. Closing one New Tab does not stop the profile service.

Operational values are the implementation's initial bounded defaults, not a new
user-approved product promise. The specification's D-01–D-05/D-09 decisions and
actual browser QA remain the source for changes. The browser test modules cover
parser safety, identity, cache/state behavior, reopening, and scheduler bounds.
