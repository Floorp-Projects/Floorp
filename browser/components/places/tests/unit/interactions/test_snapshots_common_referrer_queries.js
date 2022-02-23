/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests of queries on common referrer snapshots (i.e. those with a referrer common to the context url)
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
});

/**
 * Reset all interactions and snapshots
 *
 */
async function reset_interactions_snapshots() {
  await Interactions.reset();
  await Snapshots.reset();
}

/**
 * Creates a an interaction and snapshot for a given url
 *
 * @param {object} [options]
 * @param {string} [options.url]
 *   The url
 * @param {string} [options.referrer]
 *   The referrer url
 */
async function create_interaction_and_snapshot({
  url = "https://example.com/",
  referrer = "",
} = {}) {
  await addInteractions([
    {
      url,
      referrer,
    },
  ]);

  await Snapshots.add({ url });
}

// Neither visits came from a referrer, so no snapshots should be found
add_task(async function test_query_no_referrer() {
  await reset_interactions_snapshots();

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_a",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_b",
  });
});

// The interactions came from different referrers, so no snapshots should be found
add_task(async function test_query_no_common_referrer() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer urls so they are in the places db
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
    {
      url: "https://example.com/",
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
    referrer: "https://example.com/product_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
    referrer: "https://example.com",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_a",
    referrerUrl: "https://example.com/product_search",
  });

  await assertCommonReferrerSnapshots([], {
    url: "https://example.com/product_b",
    referrerUrl: "https://example.com/",
  });
});

// The snapshot has an interaction with referrer common to the context url, so it should be found
add_task(async function test_query_common_referrer() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer url
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
    referrer: "https://example.com/product_search",
  });

  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
    referrer: "https://example.com/product_search",
  });

  // No interactions with a common referrer for /product_c
  await create_interaction_and_snapshot({
    url: "https://example.com/product_c",
  });

  let context = {
    url: "https://example.com/product_a",
    referrerUrl: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_b", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  context = {
    url: "https://example.com/product_b",
    referrerUrl: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  // Snapshots should still be found for https://example.com/product_c since the context shares a common referrer with the snapshots
  context = {
    url: "https://example.com/product_c",
    referrerUrl: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots(
    [
      { url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 },
      { url: "https://example.com/product_b", commonReferrerScoreEqualTo: 1.0 },
    ],
    context
  );
});

// The snapshots have multiple interactions, each with different referrers.
// Snapshots should be selected when the context referrer is common to one of the interactions.
add_task(async function test_query_common_referrer_multiple_interaction() {
  await reset_interactions_snapshots();

  // Add interactions for the referrer urls so they are in the places db
  await addInteractions([
    {
      url: "https://example.com/product_search",
    },
    {
      url: "https://example.com/top_level_search",
    },
  ]);

  // Add interactions with separate referrer
  await addInteractions([
    {
      url: "https://example.com/product_a",
      referrer: "https://example.com/product_search",
      created_at: Date.now(),
    },
    {
      url: "https://example.com/product_a",
      referrer: "https://example.com/top_level_search",
      created_at: Date.now() + 60000, // we can't have two interactions for the same url at the same time
    },
  ]);

  await create_interaction_and_snapshot({
    url: "https://example.com/product_a",
  });

  // Add an interaction with a single referrers
  await addInteractions([
    {
      url: "https://example.com/product_b",
      referrer: "https://example.com/product_search",
    },
  ]);
  await create_interaction_and_snapshot({
    url: "https://example.com/product_b",
  });

  // https://example.com/product_a should be found as a snapshot with either of the referrer urls
  let context = {
    url: "https://example.com/product_b",
    referrerUrl: "https://example.com/product_search",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  context = {
    url: "https://example.com/product_b",
    referrerUrl: "https://example.com/top_level_search",
  };
  await assertCommonReferrerSnapshots(
    [{ url: "https://example.com/product_a", commonReferrerScoreEqualTo: 1.0 }],
    context
  );

  // No snapshot should be found if an unrelated referrer is used
  context = {
    url: "https://example.com/product_b",
    referrerUrl: "https://example.com/different_path",
  };
  await assertCommonReferrerSnapshots([], context);
});

// The snapshots have multiple interactions and the page data is correctly retrieved
add_task(
  async function test_query_common_referrer_multiple_interactions_page_data() {
    await reset_interactions_snapshots();

    // Register some page data.
    PageDataService.pageDataDiscovered({
      url: "https://example.com/product_a",
      date: Date.now(),
      siteName: "Example site name",
      description: "Example site description",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          price: {
            value: 276,
            currency: "USD",
          },
        },
      },
    });

    // Add an interactions for the referrer url so it's in the places db
    await addInteractions([
      {
        url: "https://example.com/product_search",
      },
    ]);

    // Add multiple interactions with the same referrer
    await addInteractions([
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now(),
      },
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now() + 60000, // we can't have two interactions for the same url at the same time
      },
      {
        url: "https://example.com/product_a",
        referrer: "https://example.com/product_search",
        created_at: Date.now() + 120000, // we can't have two interactions for the same url at the same time
      },
    ]);

    await create_interaction_and_snapshot({
      url: "https://example.com/product_a",
    });

    // Add an interaction with a single referrers
    await addInteractions([
      {
        url: "https://example.com/product_b",
        referrer: "https://example.com/product_search",
      },
    ]);
    await create_interaction_and_snapshot({
      url: "https://example.com/product_b",
    });

    let context = {
      url: "https://example.com/product_b",
      referrerUrl: "https://example.com/product_search",
    };

    let snapshot = await Snapshots.queryCommonReferrer(
      context.url,
      context.referrerUrl
    );

    Assert.equal(snapshot.length, 1, "One shapshot should be found");
    Assert.equal(
      snapshot[0].url,
      "https://example.com/product_a",
      "Correct snapshot should be found"
    );
    Assert.equal(
      snapshot[0].siteName,
      "Example site name",
      "Site name should be found"
    );
    Assert.equal(
      snapshot[0].description,
      "Example site description",
      "Site description should be found"
    );
    Assert.equal(
      snapshot[0].pageData.size,
      1,
      "Should have 1 item of page data."
    );
    Assert.deepEqual(
      snapshot[0].pageData.get(PageDataSchema.DATA_TYPE.PRODUCT),
      { price: { value: 276, currency: "USD" } },
      "Should have the right price."
    );
  }
);
