# KAN-96 original YouTube Music: actual safe-mode request blocked by managed proxy

One genuine `https://music.youtube.com/` navigation was attempted in the authenticated complete main29a3 product, native159.0.1/BuildID20261103153232, with live `appinfo.inSafeMode=true`, normalDBus/Xvfb103 and sandbox6. This is not a local substitute for the original site.

## Concrete network evidence

The normal OS-verified Python HTTPS path through the inherited proxy returned **CONNECT403 Forbidden before TLS**, with no peer certificate observed (`../music-host-tls.json`). One actual product navigation corroborated the failure: the screenshot `site.png` visibly says **“The proxy server is refusing connections”** and identifies music.youtube.com. The product document is an about:neterror page, not YouTube Music content. `content.json` records the error document/title and absence of a manifest; `browser-state.json` records the actual safe-mode process and absent install entrypoint. The site phase completed in2.94seconds.

The browser used standard dedicated-profile proxy preferences selecting the inherited managed proxy, with explicit `acceptInsecureCerts=false`. The existing public environment-proxy CA was first confirmed byte-for-byte by SHA256 to be present in normal OS default trust, then registered in only this dedicated NSS profile. Fingerprint C7:1B:4D:1E:9D:77:75:C5:38:C6:88:AB:10:CE:A9:CE:41:7D:73:12:E8:98:38:F9:D1:5F:F7:32:56:CD:7F:C3. No arbitrary CA, certificate exception, TLS validation disable, alternate host, proxy bypass, authentication/consent action, playback or TryAgain click was used. CA PEM/certificate databases and profiles are excluded from publication.

## Classification

**Original-site environment/network blocked**, specifically managed-proxy CONNECT403. Do not classify this as a product bug, an original-site successful non-reproduction, or an authorization/geo/login diagnosis. The controlled COOP/COEP safe-mode actor/creation/cold-launch cases succeeded separately; they do not qualify the blocked real site.

The error page's `getActor` observation throws `NotSupportedError` because the registered HTTP(S) PWA actor does not match the actual **about:neterror document**. That is an infrastructure error-page consequence and is not a reproduction of the reported actor mismatch on real YouTube Music content. No PWA installation was attempted on the error page.

Remaining condition: actual YouTube Music page/manifest/actor/PWA creation in genuine safe mode after the normal managed proxy can establish the site's HTTPS tunnel, plus original CachyOS/Arch package, reporter profile and full desktop/permissions. No alternate route was tried. Existing #2680 remains the actor fix; no speculative source patch or new PR is justified.

One normal Quit exit0/forcedfalse/in_app=true, all product hashes unchanged, owned processes0/display103 absent. URLs in JSON/log evidence have query/fragment values removed; screenshots show only the originally requested public site. No user data, account, CA database, profile contents or credentials are published. OriginalLinux22 denominator is unchanged.
