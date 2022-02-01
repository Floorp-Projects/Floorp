/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the page data service can parse schema.org metadata into PageData.
 */

add_task(async function test_single_product_microdata() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <title>Product Info 1</title>
      </head>
      <body>
        <div itemscope itemtype="https://schema.org/Organization">
          <div itemprop="employee" itemscope itemtype="https://schema.org/Person">
            <span itemprop="name">Mr. Nested Name</span>
          </div>

          <span itemprop="name">Mozilla</span>
        </div>

        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>

          <div itemprop="offers" itemscope itemtype="https://schema.org/Offer">
            <span itemprop="price" content="3.50">£3.50</span>
            <span itemprop="priceCurrency" content="GBP"></span>
          </div>

          <span itemprop="gtin" content="13572468"></span>

          <span itemprop="description">The most amazing microwave in the world</span>
        </div>
      </body>
      </html>
    `,
    {
      siteName: "Mozilla",
      description: "The most amazing microwave in the world",
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3.5,
            currency: "GBP",
          },
        },
      },
    }
  );
});

add_task(async function test_single_product_json_ld() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <script type="application/ld+json">
        {
          "@context": "http://schema.org",
          "@type": "Organization",
          "employee": {
            "@type": "Person",
            "name": "Mr. Nested Name"
          },
          "name": "Mozilla"
        }
      </script>
      <script type="application/ld+json">
        {
          "@context": "https://schema.org",
          "@type": "Product",
          "image": "bon-echo-microwave-17in.jpg",
          "url": "microwave.html",
          "name": "Bon Echo Microwave",
          "offers": {
            "@type": "Offer",
            "price": "3.50",
            "priceCurrency": "GBP"
          },
          "gtin": "13572468",
          "description": "The most amazing microwave in the world"
        }
      </script>
      </head>
      <body>
      </body>
      </html>
    `,
    {
      siteName: "Mozilla",
      description: "The most amazing microwave in the world",
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3.5,
            currency: "GBP",
          },
        },
      },
    }
  );
});

add_task(async function test_single_product_combined() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <script type="application/ld+json">
        {
          "@context": "https://schema.org",
          "@type": "Product",
          "image": "bon-echo-microwave-17in.jpg",
          "url": "microwave.html",
          "name": "Bon Echo Microwave",
          "offers": {
            "@type": "Offer",
            "price": "3.50",
            "priceCurrency": "GBP"
          },
          "gtin": "13572468",
          "description": "The most amazing microwave in the world"
        }
      </script>
      </head>
      <body>
        <div itemscope itemtype="https://schema.org/Organization">
          <div itemprop="employee" itemscope itemtype="https://schema.org/Person">
            <span itemprop="name">Mr. Nested Name</span>
          </div>

          <span itemprop="name">Mozilla</span>
        </div>
      </body>
      </html>
    `,
    {
      siteName: "Mozilla",
      description: "The most amazing microwave in the world",
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3.5,
            currency: "GBP",
          },
        },
      },
    }
  );
});

add_task(async function test_single_multiple_microdata() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
      <title>Product Info 2</title>
      </head>
      <body>
        <div itemscope itemtype="https://schema.org/Product">
          <img itemprop="image" src="bon-echo-microwave-17in.jpg" />
          <a href="microwave.html" itemprop="url">
            <span itemprop="name">Bon Echo Microwave</span>
          </a>

          <div itemprop="offers" itemscope itemtype="https://schema.org/Offer">
            <span itemprop="price" content="3.28">£3.28</span>
            <span itemprop="priceCurrency" content="GBP"></span>
          </div>

          <span itemprop="gtin" content="13572468"></span>
        </div>
        <div itemscope itemtype="http://schema.org/Product">
          <img itemprop="image" src="gran-paradiso-toaster-17in.jpg" />
          <a href="toaster.html" itemprop="url">
            <span itemprop="name">Gran Paradiso Toaster</span>
          </a>

          <span itemprop="gtin" content="15263748"></span>
        </div>
      </body>
      </html>
    `,
    {
      image: BASE_URL + "/bon-echo-microwave-17in.jpg",
      data: {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3.28,
            currency: "GBP",
          },
        },
      },
    }
  );
});
