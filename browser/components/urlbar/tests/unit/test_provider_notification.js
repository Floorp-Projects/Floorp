/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let firstProvider;
let secondProvider;
let context;

add_setup(async function () {
  firstProvider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "https://mozilla.com/",
          tags: [],
          title: "mozilla.com",
          displayUrl: "mozilla.com",
        }
      ),
    ],
    priority: 999,
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
    name: "firstProvider",
    onEngagement: () => {},
    onAbandonment: () => {},
    onImpression: () => {},
    onSearchSessionEnd: () => {},
    onLegacyEngagement: () => {},
  });

  secondProvider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "https://example.com/",
          tags: [],
          title: "example.com",
          displayUrl: "example.com",
        }
      ),
    ],
    priority: 999,
    type: UrlbarUtils.PROVIDER_TYPE.PROFILE,
    name: "secondProvider",
  });

  context = createContext("", {
    providers: [firstProvider.name, secondProvider.name],
  });

  UrlbarProvidersManager.registerProvider(firstProvider);
  UrlbarProvidersManager.registerProvider(secondProvider);

  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(firstProvider);
    UrlbarProvidersManager.unregisterProvider(secondProvider);
    sinon.restore();
  });
});

add_task(async function testOnEngagementNotification() {
  let spyFirstProviderOnEngagement = sinon.spy(firstProvider, "onEngagement");

  const engagedResult = makeVisitResult(context, {
    uri: "https://mozilla.com/",
    providerName: "firstProvider",
    source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
  });

  await check_results({
    context,
    matches: [
      engagedResult,
      makeVisitResult(context, {
        uri: "https://example.com/",
        providerName: "secondProvider",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      }),
    ],
  });

  let controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
  });

  await UrlbarProvidersManager.notifyEngagementChange(
    "engagement",
    context,
    {
      result: engagedResult,
    },
    controller
  );

  Assert.equal(
    spyFirstProviderOnEngagement.callCount,
    1,
    "onEngagement called once for first provider"
  );
});

add_task(async function testOnAbandonmentNotification() {
  let spyFirstProviderOnAbandonment = sinon.spy(firstProvider, "onAbandonment");

  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "https://mozilla.com/",
        providerName: "firstProvider",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/",
        providerName: "secondProvider",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      }),
    ],
  });

  let controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
  });

  await UrlbarProvidersManager.notifyEngagementChange(
    "abandonment",
    context,
    {},
    controller
  );

  Assert.equal(
    spyFirstProviderOnAbandonment.callCount,
    1,
    "onAbandonment called once for first provider"
  );
});

add_task(async function testOnImpressionNotification() {
  let spyFirstProviderOnImpression = sinon.spy(firstProvider, "onImpression");

  const engagedResult = makeVisitResult(context, {
    uri: "https://mozilla.com/",
    providerName: "firstProvider",
    source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
  });

  await check_results({
    context,
    matches: [
      engagedResult,
      makeVisitResult(context, {
        uri: "https://example.com/",
        providerName: "secondProvider",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      }),
    ],
  });

  let controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
  });

  await UrlbarProvidersManager.notifyEngagementChange(
    "engagement",
    context,
    {
      isSessionOngoing: false,
      result: engagedResult,
    },
    controller
  );

  Assert.equal(
    spyFirstProviderOnImpression.callCount,
    1,
    "onImpression called for first provider after an engagement event"
  );

  await UrlbarProvidersManager.notifyEngagementChange(
    "abandonment",
    context,
    {
      isSessionOngoing: false,
    },
    controller
  );

  Assert.equal(
    spyFirstProviderOnImpression.callCount,
    2,
    "onImpression called once more for first provider after an abandonment \
     event"
  );
});

add_task(async function testOnSearchSessionEndNotification() {
  let spyFirstProviderOnSearchSessionEnd = sinon.spy(
    firstProvider,
    "onSearchSessionEnd"
  );

  const engagedResult = makeVisitResult(context, {
    uri: "https://mozilla.com/",
    providerName: "firstProvider",
    source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
  });

  await check_results({
    context,
    matches: [
      engagedResult,
      makeVisitResult(context, {
        uri: "https://example.com/",
        providerName: "secondProvider",
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      }),
    ],
  });

  let controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
  });

  await UrlbarProvidersManager.notifyEngagementChange(
    "engagement",
    context,
    {
      isSessionOngoing: false,
      result: engagedResult,
    },
    controller
  );

  Assert.equal(
    spyFirstProviderOnSearchSessionEnd.callCount,
    1,
    "onSearchSessionEnd called for first provider after an engagement event"
  );

  await UrlbarProvidersManager.notifyEngagementChange(
    "abandonment",
    context,
    {
      isSessionOngoing: false,
    },
    controller
  );

  Assert.equal(
    spyFirstProviderOnSearchSessionEnd.callCount,
    2,
    "onSearchSessionEnd called once more for first provider after an \
     abandonment event"
  );
});

add_task(async function testProviderPresenceInMap() {
  const notificationMethods = [
    "onEngagement",
    "onAbandonment",
    "onImpression",
    "onSearchSessionEnd",
    "onLegacyEngagement",
  ];

  for (const method of notificationMethods) {
    const providersForMethod =
      UrlbarProvidersManager.providersByNotificationType[method];

    const isFirstProviderPresent = providersForMethod.has(firstProvider);
    const isSecondProviderPresent = providersForMethod.has(secondProvider);

    Assert.ok(
      isFirstProviderPresent,
      `The key ${method} includes the firstProvider`
    );
    Assert.ok(
      !isSecondProviderPresent,
      `The key ${method} does not include secondProvider`
    );
  }

  UrlbarProvidersManager.unregisterProvider(firstProvider);

  for (const method of notificationMethods) {
    const providersForMethod =
      UrlbarProvidersManager.providersByNotificationType[method];
    const isPresent = providersForMethod.has(firstProvider);
    Assert.ok(
      !isPresent,
      `FirstProvider should no longer be present for ${method} after being
       unregistered`
    );
  }
});
