# FDH Workspace OAuth — live operator probe (2026-11-21)

First live operator run of `fdh auth probe` against the real Google provider,
following the procedure in `CHECKPOINT.md`.

## Environment

- Google OAuth client: `FDH Workspace SSO - Local Validation` (web application,
  project `floorp-workspace-cli`, client ID `706050617198-4gii...teqtb`)
- Consent screen audience: Internal — `floorp.app` Workspace members only
- Redirect URI: `http://127.0.0.1:8765/oauth/callback` (pre-registered)
- Client secret: newly minted for this run; stored as the organization secret
  `FDH_GOOGLE_CLIENT_SECRET`
- Operator identity: a `floorp.app` Google Workspace member
  (account-specific values redacted below)

## Command

```sh
deno run -A tools/fdh.ts auth probe \
  --client-id 706050617198-4giierlpga5b7rulbd8q9v9r0j4teqtb.apps.googleusercontent.com \
  --redirect-uri http://127.0.0.1:8765/oauth/callback \
  --client-secret-env FDH_GOOGLE_CLIENT_SECRET
```

## Result (CLI output, account-specific values redacted)

```json
{"schemaVersion":1,"scope":"workspace-oauth-probe","codeExchangeVerified":true,"refreshVerified":true,"localLogout":true,"providerRevocationConfirmed":true,"identity":{"subject":"REDACTED","email":"REDACTED","hostedDomain":"floorp.app","issuer":"https://accounts.google.com"},"mcpAuthorizationGranted":false,"ac05Qualified":false}
```

The normal, refresh and revocation cases passed live against Google.
`ac05Qualified` and `mcpAuthorizationGranted` remain `false`: MCP-side
resource authorization (spike test B, an OAuth-protected staging MCP
endpoint) is still required for AC-05.
