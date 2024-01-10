/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests search engine aliases. See
 * browser/components/urlbar/tests/browser/browser_tokenAlias.js for tests of
 * the token alias list (i.e. showing all aliased engines on a "@" query).
 */

testEngine_setup();

// Basic test that uses two engines, a GET engine and a POST engine, neither
// providing search suggestions.
add_task(async function basicGetAndPost() {
  await SearchTestUtils.installSearchExtension({
    name: "AliasedGETMozSearch",
    keyword: "get",
    search_url: "https://s.example.com/search",
  });
  await SearchTestUtils.installSearchExtension({
    name: "AliasedPOSTMozSearch",
    keyword: "post",
    search_url: "https://s.example.com/search",
    search_url_post_params: "q={searchTerms}",
  });

  for (let alias of ["get", "post"]) {
    let context = createContext(alias, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          heuristic: true,
          providerName: "HeuristicFallback",
        }),
      ],
    });

    context = createContext(`${alias} `, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} fire`, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "fire",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} mozilla`, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "mozilla",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} MoZiLlA`, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "MoZiLlA",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} mozzarella mozilla`, {
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "mozzarella mozilla",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} kitten?`, {
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "kitten?",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });

    context = createContext(`${alias} kitten ?`, {
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: `Aliased${alias.toUpperCase()}MozSearch`,
          query: "kitten ?",
          alias,
          heuristic: true,
          providerName: "AliasEngines",
        }),
      ],
    });
  }

  await cleanupPlaces();
});
