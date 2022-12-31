# Guide to targeting with JEXL

For a more in-depth explanation of JEXL syntax you can read the [Normady project docs](https://mozilla.github.io/normandy/user/filters.html?highlight=jexl).

## How to write JEXL targeting expressions
A message needs to contain the `targeting` property (JEXL string) which is evaluated against the provided attributes.
Examples:

```javascript
{
  "id": "7864",
  "content": {...},
  // simple equality check
  "targeting": "usesFirefoxSync == true"
}

{
  "id": "7865",
  "content": {...},
  // using JEXL transforms and combining two attributes
  "targeting": "usesFirefoxSync == true && profileAgeCreated > '2018-01-07'|date"
}

{
  "id": "7866",
  "content": {...},
  // targeting addon information
  "targeting": "addonsInfo.addons['activity-stream@mozilla.org'].name == 'Activity Stream'"
}

{
  "id": "7866",
  "content": {...},
  // targeting based on time
  "targeting": "currentDate > '2018-08-08'|date"
}
```
