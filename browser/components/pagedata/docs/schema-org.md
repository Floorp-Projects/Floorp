# Schema.org page data

Collects data marked up in pages in the [schema.org](https://schema.org/) microdata format.

## Product

Collects product information from the page. Since a page may contain multiple products the data
type is an array.

```json
{
  "type": PageDataCollector.DATA_TYPE.PRODUCT,
  "data": [
    {
      "gtin": <The Global Trade Item Number for the product>,
      "name": <The name of the product>,
      "image": <An image for the product>,
      "url": <A canonical url for the product>,
      "price": <The price of the product>,
      "currency": <The currency of the product (ISO 4217)>,
    },

    ... more products
  ]
}
```
