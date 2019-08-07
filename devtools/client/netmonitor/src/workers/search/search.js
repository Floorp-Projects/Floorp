/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable no-unused-vars */

"use strict";

/**
 * Search within specified resource. Note that this function runs
 * within a worker thread.
 */
function searchInResource(resource, query) {
  const results = [];

  if (resource.url) {
    results.push(
      findMatches(resource, query, {
        key: "url",
        label: "Url",
        type: "url",
      })
    );
  }

  if (resource.responseHeaders) {
    results.push(
      findMatches(resource, query, {
        key: "responseHeaders.headers",
        label: "Response Headers",
        type: "responseHeaders",
      })
    );
  }

  if (resource.requestHeaders) {
    results.push(
      findMatches(resource, query, {
        key: "requestHeaders.headers",
        label: "Request Headers",
        type: "requestHeaders",
      })
    );
  }

  if (resource.responseCookies) {
    let key = "responseCookies";

    if (resource.responseCookies.cookies) {
      key = "responseCookies.cookies";
    }

    results.push(
      findMatches(resource, query, {
        key,
        label: "Response Cookies",
        type: "responseCookies",
      })
    );
  }

  if (resource.requestCookies) {
    let key = "requestCookies";

    if (resource.requestCookies.cookies) {
      key = "requestCookies.cookies";
    }

    results.push(
      findMatches(resource, query, {
        key,
        label: "Request Cookies",
        type: "requestCookies",
      })
    );
  }

  if (resource.securityInfo) {
    results.push(
      findMatches(resource, query, {
        key: "securityInfo",
        label: "Security Information",
        type: "securityInfo",
      })
    );
  }

  if (resource.responseContent) {
    results.push(
      findMatches(resource, query, {
        key: "responseContent.content.text",
        label: "Response Content",
        type: "responseContent",
      })
    );
  }

  if (resource.requestPostData) {
    results.push(
      findMatches(resource, query, {
        key: "requestPostData.postData.text",
        label: "Request Post Data",
        type: "requestPostData",
      })
    );
  }

  return getResults(results);
}

/**
 * Concatenates all results
 * @param results
 * @returns {*[]}
 */
function getResults(results) {
  const tempResults = [].concat.apply([], results);

  // Generate unique result keys
  tempResults.forEach((result, index) => {
    result.key = index;
  });

  return tempResults;
}

/**
 * Find query matches in arrays, objects and strings.
 * @param resource
 * @param query
 * @param data
 * @returns {*}
 */
function findMatches(resource, query, data) {
  if (!resource || !query || !data) {
    return [];
  }

  const resourceValue = getValue(data.key, resource);
  const resourceType = getType(resourceValue);

  switch (resourceType) {
    case "string":
      return searchInText(query, resourceValue, data);
    case "array":
      return searchInArray(query, resourceValue, data);
    case "object":
      return searchInObject(query, resourceValue, data);
    default:
      return [];
  }
}

/**
 * Get type of resource - deals with arrays as well.
 * @param resource
 * @returns {*}
 */
function getType(resource) {
  return Array.isArray(resource) ? "array" : typeof resource;
}

/**
 * Function returns the value of a key, included nested keys.
 * @param path
 * @param obj
 * @returns {*}
 */
function getValue(path, obj) {
  const properties = Array.isArray(path) ? path : path.split(".");
  return properties.reduce((prev, curr) => prev && prev[curr], obj);
}

/**
 * Search text for specific string and return all matches found
 * @param query
 * @param text
 * @param data
 * @returns {*}
 */
function searchInText(query, text, data) {
  const { type, label } = data;
  const lines = text.split(/\r\n|\r|\n/);
  const matches = [];

  // iterate through each line
  lines.forEach((curr, i, arr) => {
    const regexQuery = RegExp(query, "gmi");
    let singleMatch;

    while ((singleMatch = regexQuery.exec(lines[i])) !== null) {
      const startIndex = regexQuery.lastIndex;
      matches.push({
        type,
        label,
        line: i,
        value: getTruncatedValue(lines[i], query, startIndex),
        startIndex,
      });
    }
  });

  return matches.length === 0 ? [] : matches;
}

/**
 * Search for query in array.
 * Iterates through each array item and handles item based on type.
 * @param query
 * @param arr
 * @param data
 * @returns {*}
 */
function searchInArray(query, arr, data) {
  const { type, key, label } = data;

  const matches = arr
    .filter(match => JSON.stringify(match).includes(query))
    .map((match, i) =>
      findMatches(match, query, {
        label,
        key: key + ".[" + i + "]",
        type,
      })
    );

  return getResults(matches);
}

/**
 * Return query match and up to 100 characters on left and right.
 * (100) + [matched query] + (100)
 * @param value
 * @param query
 * @param startIndex
 * @returns {*}
 */
function getTruncatedValue(value, query, startIndex) {
  const valueSize = value.length;
  const indexEnd = startIndex + query.length;

  if (valueSize < 200 + query.length) {
    return value;
  }

  const start = value.substring(startIndex, startIndex - 100);
  const end = value.substring(indexEnd, indexEnd + 100);

  return start + query + end;
}

/**
 * Iterates through object, including nested objects, returns all
 * found matches.
 * @param query
 * @param obj
 * @param data
 * @returns {*}
 */
function searchInObject(query, obj, data) {
  const { type, label } = data;
  const matches = data.hasOwnProperty("collector") ? data.collector : [];

  for (const objectKey in obj) {
    if (obj.hasOwnProperty(objectKey)) {
      if (typeof obj[objectKey] === "object") {
        searchInObject(obj[objectKey], query, {
          collector: matches,
          type,
          label: objectKey,
        });
      }

      let location;

      if (objectKey) {
        if (objectKey.includes(query)) {
          location = "name";
        } else if (
          typeof obj[objectKey] === "string" &&
          obj[objectKey].includes(query)
        ) {
          location = "value";
        }
      }

      if (!location) {
        continue;
      }

      const match = {
        type,
        label: objectKey,
      };

      const value = location === "name" ? objectKey : obj[objectKey];
      const startIndex = value.indexOf(query);

      match.startIndex = startIndex;
      match.value = getTruncatedValue(value, query, startIndex);

      matches.push(match);
    }
  }

  return matches;
}
