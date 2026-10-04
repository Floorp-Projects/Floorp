# Floorp OS native click contract

`POST /tabs/instances/:id/click` and
`POST /scraper/instances/:id/click` accept a CSS `selector` or an extracted
`fingerprint`. A selector takes precedence when both are supplied.

```json
{
  "selector": "#submit",
  "button": "left",
  "clickCount": 1,
  "force": false,
  "timeout": 5000,
  "stabilityTimeout": 100,
  "includeResult": true
}
```

`button` accepts `left`, `middle`, or `right`. `clickCount` accepts 1 or 2.
`timeout` and `stabilityTimeout` are finite, nonnegative milliseconds. The
defaults are 5000 and 100; the action timeout is capped at 60000. Zero timeout
refuses without input; zero stability timeout skips the stability wait.
Invalid option values return HTTP 400 before dispatch.

The timeout begins when the child actor starts the action. Actor lookup,
fingerprint resolution, IPC, and HTTP transport can add time outside that
budget. A client timeout does not cancel an already queued action.

The action waits for one connected target that is visible, enabled, unobscured
at its center, and stable within a two CSS pixel tolerance for the requested
interval. Replacing the target resets stability. Open shadow roots and slotted
content participate in hit testing. Ambiguous selectors are refused even with
`force: true`. Force skips actionability, scrolling, and stability checks, so
it can deliver at coordinates that do not activate the selected element.

Input uses one privileged mouse sequence with no DOM `.click()` fallback and
no retry after delivery starts. A double click sends two down/up pairs and
rechecks eligibility before the second pair. The managed tab's own control
overlay is suspended only during synchronous hit testing and input; website
overlays remain part of the actionability check. The control overlay continues
to block user interaction between polling waits.

Without `includeResult`, the response stays `{ "ok": true }` or
`{ "ok": false }`. Existing `clickElement()` service calls still return a
boolean. `clickElementWithResult()` and `includeResult: true` expose:

| Field | Meaning |
| --- | --- |
| `ok` | True only when the requested native sequence completed. |
| `status` | `dispatched`, `refused`, `unsupported`, or `unknown`. |
| `reason` | Diagnostic description of completion or failure. |
| `phase` | Last preparation, wait, or input phase; null if no actor result exists. |
| `inputStarted` | Whether mouse delivery was attempted; null if unknown. |
| `activationStarted` | Whether mousedown was attempted; null if unknown. |
| `backend` | `window-synthesizeMouseEvent`. |

`dispatched` confirms delivery, not the website's resulting state or completion
of a larger task. `refused` may follow mousemove, which itself can have page
effects. `unknown` can follow partial activation or loss of the actor result;
clients must inspect the page before deciding whether another operation is
appropriate. Blindly retrying can duplicate an effect. Missing native input
capability returns `unsupported` without a legacy fallback.

The endpoint does not promise atomicity against arbitrary page script or
exactly-once task completion. A page can change during mouse handlers, and an
already-started down/up pair completes its release even if the deadline expires
during the down handler.

Run the product path checks against an isolated Floorp instance with its OS
server enabled:

```sh
deno run --allow-net --allow-env tools/os-test/verify_native_click_contract.ts \
  --base-url http://127.0.0.1:58261 --token-env NATIVE_CLICK_TEST_TOKEN
```

The runner creates a private fixture server and its own tab/scraper instances,
then removes them. Browser state tests are colocated under
`browser-features/modules/actors/webscraper/test/`.
