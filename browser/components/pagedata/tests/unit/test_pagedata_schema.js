/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests schema validation.
 */

add_task(async function testBasic() {
  // Old data types, should not be recognised.
  Assert.equal(PageDataSchema.nameForType(1), null);
  Assert.equal(PageDataSchema.nameForType(2), null);

  Assert.equal(
    PageDataSchema.nameForType(PageDataSchema.DATA_TYPE.VIDEO),
    "VIDEO"
  );
  Assert.equal(
    PageDataSchema.nameForType(PageDataSchema.DATA_TYPE.PRODUCT),
    "PRODUCT"
  );
});

add_task(async function testProduct() {
  // Products must have a name
  await Assert.rejects(
    PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {}),
    /missing required property 'name'/
  );

  await PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
    name: "Bolts",
  });

  await PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
    name: "Bolts",
    price: {
      value: 5,
    },
  });

  await PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
    name: "Bolts",
    price: {
      value: 5,
      currency: "USD",
    },
  });

  await Assert.rejects(
    PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
      name: "Bolts",
      price: {
        currency: "USD",
      },
    }),
    /missing required property 'value'/
  );

  await PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
    name: "Bolts",
    shippingCost: {
      value: 5,
      currency: "USD",
    },
  });

  await Assert.rejects(
    PageDataSchema.validateData(PageDataSchema.DATA_TYPE.PRODUCT, {
      name: "Bolts",
      shippingCost: {
        currency: "USD",
      },
    }),
    /missing required property 'value'/
  );
});

add_task(async function testCoalesce() {
  let joined = PageDataSchema.coalescePageData({}, {});
  Assert.deepEqual(joined, { data: {} });

  joined = PageDataSchema.coalescePageData(
    {
      url: "https://www.google.com/",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "bolts",
        },
        [PageDataSchema.DATA_TYPE.VIDEO]: {
          name: "My video",
          duration: 500,
        },
      },
    },
    {
      url: "https://www.mozilla.com/",
      date: 27,
      siteName: "Mozilla",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "newname",
          price: {
            value: 55,
          },
        },
        [PageDataSchema.DATA_TYPE.AUDIO]: {
          name: "My song",
        },
      },
    }
  );

  Assert.deepEqual(joined, {
    url: "https://www.google.com/",
    date: 27,
    siteName: "Mozilla",
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        name: "bolts",
        price: {
          value: 55,
        },
      },
      [PageDataSchema.DATA_TYPE.VIDEO]: {
        name: "My video",
        duration: 500,
      },
      [PageDataSchema.DATA_TYPE.AUDIO]: {
        name: "My song",
      },
    },
  });
});

add_task(async function testPageData() {
  // Full page data needs a url and a date
  await Assert.rejects(
    PageDataSchema.validatePageData({}),
    /missing required property 'url'/
  );

  await Assert.rejects(
    PageDataSchema.validatePageData({ url: "https://www.google.com" }),
    /missing required property 'date'/
  );

  await Assert.rejects(
    PageDataSchema.validatePageData({ date: 55 }),
    /missing required property 'url'/
  );

  Assert.deepEqual(
    await PageDataSchema.validatePageData({
      url: "https://www.google.com",
      date: 55,
    }),
    { url: "https://www.google.com", date: 55, data: {} }
  );

  Assert.deepEqual(
    await PageDataSchema.validatePageData({
      url: "https://www.google.com",
      date: 55,
      data: {
        0: {
          name: "unknown",
        },
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bolts",
          price: {
            value: 55,
          },
        },
      },
    }),
    {
      url: "https://www.google.com",
      date: 55,
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bolts",
          price: {
            value: 55,
          },
        },
      },
    }
  );

  // Should drop invalid inner data.
  Assert.deepEqual(
    await PageDataSchema.validatePageData({
      url: "https://www.google.com",
      date: 55,
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bolts",
          price: {
            currency: "USD",
          },
        },
      },
    }),
    {
      url: "https://www.google.com",
      date: 55,
      data: {},
    }
  );
});
