add_task(async function test_encoded() {
  info("Searching for over encoded url should not break it");
  let url = "https://www.mozilla.com/search/top/?q=%25%32%35";
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI(url),
    title: url,
  });
  let context = createContext(url, { isPrivate: false });
  await check_results({
    context,
    autofilled: url,
    completed: url,
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: url,
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_encoded_trimmed() {
  info("Searching for over encoded url should not break it");
  let url = "https://www.mozilla.com/search/top/?q=%25%32%35";
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI(url),
    title: url,
  });
  let context = createContext("mozilla.com/search/top/?q=%25%32%35", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: "mozilla.com/search/top/?q=%25%32%35",
    completed: url,
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: url,
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_encoded_partial() {
  info("Searching for over encoded url should not break it");
  let url = "https://www.mozilla.com/search/top/?q=%25%32%35";
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI(url),
    title: url,
  });
  let context = createContext("https://www.mozilla.com/search/top/?q=%25", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: url,
    completed: url,
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: url,
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_encoded_path() {
  info("Searching for over encoded url should not break it");
  let url = "https://www.mozilla.com/%25%32%35/top/";
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI(url),
    title: url,
  });
  let context = createContext("https://www.mozilla.com/%25%32%35/t", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: url,
    completed: url,
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: url,
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});
