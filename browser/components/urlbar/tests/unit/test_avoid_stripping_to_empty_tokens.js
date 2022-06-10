/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

testEngine_setup();

add_task(async function test_protocol_trimming() {
  for (let prot of ["http", "https"]) {
    let visit = {
      // Include the protocol in the query string to ensure we get matches (see bug 1059395)
      uri: Services.io.newURI(
        prot +
          "://www.mozilla.org/test/?q=" +
          prot +
          encodeURIComponent("://") +
          "www.foo"
      ),
      title: "Test title",
    };
    await PlacesTestUtils.addVisits(visit);

    let input = prot + "://www.";
    info("Searching for: " + input);
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      autofilled: prot + "://www.mozilla.org/",
      completed: prot + "://www.mozilla.org/",
      hasAutofillTitle: false,
      matches: [
        makeVisitResult(context, {
          uri: prot + "://www.mozilla.org/",
          title:
            prot == "http" ? "www.mozilla.org" : prot + "://www.mozilla.org",
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: visit.uri.spec,
          title: visit.title,
        }),
      ],
    });

    input = "www.";
    info("Searching for: " + input);
    context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      autofilled: "www.mozilla.org/",
      completed: prot + "://www.mozilla.org/",
      hasAutofillTitle: false,
      matches: [
        makeVisitResult(context, {
          uri: prot + "://www.mozilla.org/",
          title:
            prot == "http" ? "www.mozilla.org" : prot + "://www.mozilla.org",
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: visit.uri.spec,
          title: visit.title,
        }),
      ],
    });

    input = prot + "://www. ";
    info("Searching for: " + input);
    context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          uri: `${input.trim()}/`,
          title: `${input.trim()}/`,
          iconUri: "",
          heuristic: true,
          providerName: "HeuristicFallback",
        }),
        makeVisitResult(context, {
          uri: visit.uri.spec,
          title: visit.title,
          providerName: "Places",
        }),
      ],
    });

    let inputs = [
      prot + "://",
      prot + ":// ",
      prot + ":// mo",
      prot + "://mo te",
      prot + "://www. mo",
      prot + "://www.mo te",
      "www. ",
      "www. mo",
      "www.mo te",
    ];
    for (input of inputs) {
      info("Searching for: " + input);
      context = createContext(input, { isPrivate: false });
      await check_results({
        context,
        matches: [
          makeSearchResult(context, {
            engineName: SUGGESTIONS_ENGINE_NAME,
            query: input,
            heuristic: true,
          }),
          makeVisitResult(context, {
            uri: visit.uri.spec,
            title: visit.title,
            providerName: "Places",
          }),
        ],
      });
    }

    await cleanupPlaces();
  }
});
