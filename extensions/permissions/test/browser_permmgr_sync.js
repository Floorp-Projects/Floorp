function addPerm(aOrigin, aName) {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(aOrigin);
  Services.perms.addFromPrincipal(
    principal,
    aName,
    Services.perms.ALLOW_ACTION
  );
}

add_task(async function () {
  // Make sure that we get a new process for the tab which we create. This is
  // important, because we want to assert information about the initial state
  // of the local permissions cache.

  addPerm("http://example.com", "perm1");
  addPerm("http://foo.bar.example.com", "perm2");
  addPerm("about:home", "perm3");
  addPerm("https://example.com", "perm4");
  // NOTE: This permission is a preload permission, so it should be available in
  // the content process from startup.
  addPerm("https://somerandomwebsite.com", "cookie");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank", forceNewProcess: true },
    async function (aBrowser) {
      await SpecialPowers.spawn(aBrowser, [], async function () {
        // Before the load http URIs shouldn't have been sent down yet
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "perm1"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm1-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "perm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm2-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "about:home"
            ),
            "perm3"
          ),
          Services.perms.ALLOW_ACTION,
          "perm3-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://example.com"
            ),
            "perm4"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm4-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://somerandomwebsite.com"
            ),
            "cookie"
          ),
          Services.perms.ALLOW_ACTION,
          "cookie-1"
        );

        let iframe = content.document.createElement("iframe");

        // Perform a load of example.com
        await new Promise(resolve => {
          iframe.setAttribute("src", "http://example.com");
          iframe.onload = resolve;
          content.document.body.appendChild(iframe);
        });

        // After the load finishes, the iframe process should know about example.com, but not foo.bar.example.com
        await content.SpecialPowers.spawn(iframe, [], async function () {
          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "http://example.com"
              ),
              "perm1"
            ),
            Services.perms.ALLOW_ACTION,
            "perm1-2"
          );

          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "http://foo.bar.example.com"
              ),
              "perm2"
            ),
            Services.perms.UNKNOWN_ACTION,
            "perm2-2"
          );

          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "about:home"
              ),
              "perm3"
            ),
            Services.perms.ALLOW_ACTION,
            "perm3-2"
          );

          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "https://example.com"
              ),
              "perm4"
            ),
            Services.perms.UNKNOWN_ACTION,
            "perm4-2"
          );

          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "https://somerandomwebsite.com"
              ),
              "cookie"
            ),
            Services.perms.ALLOW_ACTION,
            "cookie-2"
          );
        });

        // In Fission only, the parent process should have no knowledge about the child
        // process permissions
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "perm1"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "perm1-3"
        );

        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "perm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm2-3"
        );

        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://example.com"
            ),
            "perm4"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm4-3"
        );
      });

      addPerm("http://example.com", "newperm1");
      addPerm("http://foo.bar.example.com", "newperm2");
      addPerm("about:home", "newperm3");
      addPerm("https://example.com", "newperm4");
      addPerm("https://someotherrandomwebsite.com", "cookie");

      await SpecialPowers.spawn(aBrowser, [], async function () {
        // The new permissions should be available, but only for
        // http://example.com (without Fission), and about:home.
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "perm1"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "perm1-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "newperm1"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "newperm1-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "perm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm2-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "newperm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "newperm2-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "about:home"
            ),
            "perm3"
          ),
          Services.perms.ALLOW_ACTION,
          "perm3-3"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "about:home"
            ),
            "newperm3"
          ),
          Services.perms.ALLOW_ACTION,
          "newperm3-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://example.com"
            ),
            "perm4"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm4-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://example.com"
            ),
            "newperm4"
          ),
          Services.perms.UNKNOWN_ACTION,
          "newperm4-1"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://somerandomwebsite.com"
            ),
            "cookie"
          ),
          Services.perms.ALLOW_ACTION,
          "cookie-3"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://someotherrandomwebsite.com"
            ),
            "cookie"
          ),
          Services.perms.ALLOW_ACTION,
          "othercookie-3"
        );

        let iframe = content.document.createElement("iframe");
        // Loading a subdomain now, on https
        await new Promise(resolve => {
          iframe.setAttribute("src", "https://sub1.test1.example.com");
          iframe.onload = resolve;
          content.document.body.appendChild(iframe);
        });

        // After the load finishes, the iframe process should not know about
        // the permissions of its base domain.
        await content.SpecialPowers.spawn(iframe, [], async function () {
          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "https://example.com"
              ),
              "perm4"
            ),
            Services.perms.ALLOW_ACTION,
            "perm4-5"
          );

          // In Fission not across schemes, though.
          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "http://example.com"
              ),
              "perm1"
            ),
            SpecialPowers.useRemoteSubframes
              ? Services.perms.UNKNOWN_ACTION
              : Services.perms.ALLOW_ACTION,
            "perm1-5"
          );
          is(
            Services.perms.testPermissionFromPrincipal(
              Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                "http://example.com"
              ),
              "newperm1"
            ),
            SpecialPowers.useRemoteSubframes
              ? Services.perms.UNKNOWN_ACTION
              : Services.perms.ALLOW_ACTION,
            "newperm1-2"
          );
        });

        // The parent process should still have no permission under Fission.
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "perm1"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "perm1-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://example.com"
            ),
            "newperm1"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "newperm1-3"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://example.com"
            ),
            "perm4"
          ),
          SpecialPowers.useRemoteSubframes
            ? Services.perms.UNKNOWN_ACTION
            : Services.perms.ALLOW_ACTION,
          "perm4-6"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "perm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "perm2-5"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "http://foo.bar.example.com"
            ),
            "newperm2"
          ),
          Services.perms.UNKNOWN_ACTION,
          "newperm2-2"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "about:home"
            ),
            "perm3"
          ),
          Services.perms.ALLOW_ACTION,
          "perm3-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "about:home"
            ),
            "newperm3"
          ),
          Services.perms.ALLOW_ACTION,
          "newperm3-2"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://somerandomwebsite.com"
            ),
            "cookie"
          ),
          Services.perms.ALLOW_ACTION,
          "cookie-4"
        );
        is(
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              "https://someotherrandomwebsite.com"
            ),
            "cookie"
          ),
          Services.perms.ALLOW_ACTION,
          "othercookie-4"
        );
      });
    }
  );
});
