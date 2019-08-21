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
        panel: "headers",
      })
    );
  }

  if (resource.responseHeaders) {
    results.push(
      findMatches(resource, query, {
        key: "responseHeaders.headers",
        label: "Response Headers",
        type: "responseHeaders",
        panel: "headers",
      })
    );
  }

  if (resource.requestHeaders) {
    results.push(
      findMatches(resource, query, {
        key: "requestHeaders.headers",
        label: "Request Headers",
        type: "requestHeaders",
        panel: "headers",
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
        panel: "cookies",
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
        panel: "cookies",
      })
    );
  }

  if (resource.securityInfo) {
    results.push(
      findMatches(resource, query, {
        key: "securityInfo",
        label: "Security Information",
        type: "securityInfo",
        panel: "security",
      })
    );
  }

  if (resource.responseContent) {
    results.push(
      findMatches(resource, query, {
        key: "responseContent.content.text",
        label: "Response Content",
        type: "responseContent",
        panel: "response",
      })
    );
  }

  if (resource.requestPostData) {
    results.push(
      findMatches(resource, query, {
        key: "requestPostData.postData.text",
        label: "Request Post Data",
        type: "requestPostData",
        panel: "headers",
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

  if (resource.hasOwnProperty("name") && resource.hasOwnProperty("value")) {
    return searchInProperties(query, resource, data);
  }

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

function searchInProperties(query, obj, data) {
  const match = {
    ...data,
  };

  if (obj.name.includes(query)) {
    match.label = obj.name;
  }

  if (obj.name.includes(query) || obj.value.includes(query)) {
    match.value = obj.value;
    match.startIndex = obj.value.indexOf(query);

    return match;
  }

  return [];
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
 * @returns {Array}
 */
function searchInText(query, text, data) {
  const { type } = data;
  const lines = text.split(/\r\n|\r|\n/);
  const matches = [];

  // iterate through each line
  lines.forEach((curr, i) => {
    const regexQuery = RegExp(query, "gmi");
    const lineMatches = [];
    let singleMatch;

    while ((singleMatch = regexQuery.exec(lines[i])) !== null) {
      const startIndex = regexQuery.lastIndex;
      lineMatches.push(startIndex);
    }

    if (lineMatches.length !== 0) {
      const line = i + 1 + "";
      const match = {
        ...data,
        label: type !== "url" ? line : "Url",
        line,
        startIndex: lineMatches,
      };

      match.value =
        lineMatches.length === 1
          ? getTruncatedValue(lines[i], query, lineMatches[0])
          : lines[i];

      matches.push(match);
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
  const { key, label } = data;
  const matches = arr
    .filter(match => JSON.stringify(match).includes(query))
    .map((match, i) =>
      findMatches(match, query, {
        ...data,
        label: match.hasOwnProperty("name") ? match.name : label,
        key: key + ".[" + i + "]",
      })
    );

  return getResults(matches);
}

/**
 * Return query match and up to 50 characters on left and right.
 * (50) + [matched query] + (50)
 * @param value
 * @param query
 * @param startIndex
 * @returns {*}
 */
function getTruncatedValue(value, query, startIndex) {
  const valueSize = value.length;
  const indexEnd = startIndex + query.length;

  if (valueSize < 100 + query.length) {
    return value;
  }

  const start = value.substring(startIndex, startIndex - 50);
  const end = value.substring(indexEnd, indexEnd + 50);

  return start + end;
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
  const matches = data.hasOwnProperty("collector") ? data.collector : [];

  for (const objectKey in obj) {
    const objectKeyType = getType(obj[objectKey]);

    // if the value is an object, send to search in object
    if (objectKeyType === "object" && Object.keys(obj[objectKey].length > 1)) {
      searchInObject(query, obj[objectKey], {
        ...data,
        collector: matches,
      });
    } else if (
      (objectKeyType === "string" && obj[objectKey].includes(query)) ||
      objectKey.includes(query)
    ) {
      const match = {
        ...data,
      };

      const value = obj[objectKey];
      const startIndex = value.indexOf(query);

      match.label = objectKey;
      match.startIndex = startIndex;
      match.value = getTruncatedValue(value, query, startIndex);

      matches.push(match);
    }
  }

  return matches;
}
