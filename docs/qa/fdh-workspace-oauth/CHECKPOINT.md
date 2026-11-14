# FDH Workspace OAuth client — implementation boundary

This adds an OAuth **client**, not a new Authorization Server. D-18 remains Google Workspace OIDC plus maintained existing OAuth software; Google provider tokens are never FDH MCP tokens or runner machine credentials.

## Implementation

- `openid-client@6.8.4` is exact-pinned with the existing dependency-age guard intact. Its OAuth 4 Web API dependency performs the wire protocol; existing `jose@6.1.3` verifies Google RS256/JWKS and Workspace claims.
- Fixed Google authorization/token/revocation/key-set locations. No operator issuer/JWKS/token endpoint override. No constructor network I/O. Token POST redirects are not followed.
- Fresh state, nonce and S256 PKCE for each authorization. State is capacity-bounded, expires, and is consumed once before provider I/O, including denied or failed responses. Callback origin/path and duplicate query keys are checked. Cancellation/expiry also prevents a late exchange from installing a session.
- Initial login still requires its exact nonce. A distinct subject-bound verification path handles refreshed ID tokens without inventing an initial-login nonce. Refresh requires a fresh verified ID token rather than reusing cached Workspace membership.
- Provider access/refresh material is process-private and memory-only. The session registry owns refresh single-flight, rotation and logout invalidation. This is not distributed refresh coordination or a persistent vault.

## Explicit operator probe

```sh
deno run -A tools/fdh.ts auth probe --help
deno run -A tools/fdh.ts auth probe --client-id <public-client-id> --redirect-uri http://127.0.0.1:19281/oauth/callback
```

Register the exact loopback redirect with the selected Google client. The command prints an ephemeral Google consent URL to operator stderr; open it manually in a browser. Do not publish that URL in logs/evidence. No browser is launched automatically. If the selected Google client requires a client secret, inject it through the operator secret manager into the process and name that variable with `--client-secret-env FDH_GOOGLE_CLIENT_SECRET`. Never put the secret value in argv, chat, a checked-in config or a report.

The probe performs code exchange, refresh and local logout/provider revocation, then closes its loopback callback server. It never persists a token/session ID. `--timeout-ms` bounds browser authorization (default 300000, maximum 900000); subsequent provider requests have their own ten-second library timeout. Unknown, duplicate, bare or malformed arguments fail before environment/network access. Help is effect-free. Only the explicitly selected secret variable can be read. Revocation uncertainty is reported and yields a nonzero exit.

## Not qualified

The source/tests do **not** establish live Google acceptance, full AC-05, MCP resource/scope authorization, shared-service SSO, cross-process refresh coordination, a durable encrypted token store or central append-only audit. The CLI reports `ac05Qualified:false` and `mcpAuthorizationGranted:false`. Existing browser-local secure-profile tokens and runner identity are not replaced. No signing/deployment/publication operation is part of this probe.

A real operator run needs the public client ID and approved exact redirect, and possibly a secret supplied privately through the selected environment variable. No live run has been performed as part of implementation.
