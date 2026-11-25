# FDH AC-05 — Workspace MCP probe, live run (test B)

Date: 2026-11-21. Account: a `floorp.app` Google Workspace member (account-specific values redacted). Probe: `tools/integration/fdh_workspace_mcp_spike.ts --execute-google-workspace` against an ephemeral local Keycloak 26.8.2 container with the Google identity broker.

## Result

```json
{"scope":"local-google-workspace-mcp-probe","experimental":true,"ac05Qualified":false,"productionQualified":false,"protectedOperationsAllowed":false,"mcpAuthorizationGranted":true,"expiredTokenRejected":true,"refreshProbeSucceeded":true,"revokedTokenRejected":true,"immediateJwtInvalidationObserved":false,"immediateMcpRejectionObserved":true,"onlineSessionInactiveObserved":true,"offlineJwtStillValidAfterRevocation":true}
```

- MCP resource authorization granted through the full chain: Google consent → Keycloak broker → PKCE public client → MCP `fdh.authz.probe`.
- Expired token rejected, refresh probe succeeded, revoked token rejected at the MCP layer immediately (online introspection).
- `ac05Qualified`/`productionQualified`/`protectedOperationsAllowed` are intentionally `false`: the spike is a local experimental probe and must not authorize protected operations.
- `immediateJwtInvalidationObserved: false` + `offlineJwtStillValidAfterRevocation: true` document the expected gap: a revoked JWT stays locally verifiable until expiry (20 s in the spike); revocation is enforced only by online introspection.

## Spike fixes required by Keycloak 26.8.2

The stock spike could not run on the pinned 26.8.2 image; three real incompatibilities were found and fixed:

1. `unmanagedAttributePolicy: "DISABLED"` is no longer a valid enum value — realm profile PUT returns 400. `ADMIN_VIEW` keeps the same write restriction (users still cannot edit unmanaged attributes) while permitting management interfaces to read them, and passes `validProfile`.
2. The Admin REST API rejects service-account tokens without the built-in `kc-admin-api` client scope (RFC 9700 audience check) → `fdh-broker-reader` gets it as a default scope.
3. Token introspection requires the introspecting client to appear in the token's `aud` (hardening added in Keycloak 26.6.2). With RFC 8707 resource indicators the aud is the resource URI and never contains a client id, so the reader client needs `allow.token.introspection.without.audience.check=true`.
