/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable no-unused-vars */

"use strict";

/**
 * Search within specified resource. Note that this function runs
 * within a worker thread.
 */
function searchInResource(resource, query, modifiers) {
  const results = [];

  if (resource.url) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "url",
        label: "Url",
        type: "url",
        panel: "headers",
      })
    );
  }

  if (resource.responseHeaders) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "responseHeaders.headers",
        type: "responseHeaders",
        panel: "headers",
      })
    );
  }

  if (resource.requestHeaders) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "requestHeaders.headers",
        type: "requestHeaders",
        panel: "headers",
      })
    );
  }

  if (resource.requestHeadersFromUploadStream) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "requestHeadersFromUploadStream.headers",
        type: "requestHeadersFromUploadStream",
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
      findMatches(resource, query, modifiers, {
        key,
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
      findMatches(resource, query, modifiers, {
        key,
        type: "requestCookies",
        panel: "cookies",
      })
    );
  }

  if (resource.responseContent) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "responseContent.content.text",
        type: "responseContent",
        panel: "response",
      })
    );
  }

  if (resource.requestPostData) {
    results.push(
      findMatches(resource, query, modifiers, {
        key: "requestPostData.postData.text",
        type: "requestPostData",
        panel: "request",
      })
    );
  }

  return getResults(results, resource);
}

/**
 * Concatenates all results
 * @param results
 * @returns {*[]}
 */
function getResults(results, resource) {
  const tempResults = [].concat.apply([], results);

  // Generate unique result keys
  tempResults.forEach((result, index) => {
    result.key = index;
    result.parentResource = resource;
  });

  return tempResults;
}

function find(query, modifiers, source) {
  const { caseSensitive } = modifiers;
  const value = caseSensitive ? source : source.toLowerCase();
  const q = caseSensitive ? query : query.toLowerCase();

  return value.includes(q);
}

/**
 * Find query matches in arrays, objects and strings.
 * @param resource
 * @param query
 * @param modifiers
 * @param data
 * @returns {*[]|[]|Array|*}
 */
function findMatches(resource, query, modifiers, data) {
  if (!resource || !query || !modifiers || !data) {
    return [];
  }

  const resourceValue = getValue(data.key, resource);
  const resourceType = getType(resourceValue);

  if (resource.hasOwnProperty("name") && resource.hasOwnProperty("value")) {
    return searchInProperties(query, modifiers, resource, data);
  }

  switch (resourceType) {
    case "string":
      return searchInText(query, modifiers, resourceValue, data);
    case "array":
      return searchInArray(query, modifiers, resourceValue, data);
    case "object":
      return searchInObject(query, modifiers, resourceValue, data);
    default:
      return [];
  }
}

function searchInProperties(query, modifiers, obj, data) {
  const { name, value } = obj;
  const match = {
    ...data,
  };

  if (find(query, modifiers, name)) {
    match.label = name;
  }

  if (find(query, modifiers, name) || find(query, modifiers, value)) {
    match.value = value;
    match.startIndex = value.indexOf(query);

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
  return properties.reduce((prev, curr) => prev?.[curr], obj);
}

/**
 * Search text for specific string and return all matches found
 * @param query
 * @param modifiers
 * @param text
 * @param data
 * @returns {*}
 */
function searchInText(query, modifiers, text, data) {
  const { type } = data;
  const lines = text.split(/\r\n|\r|\n/);
  const matches = [];

  // iterate through each line
  lines.forEach((curr, i) => {
    const { caseSensitive } = modifiers;
    const flags = caseSensitive ? "g" : "gi";
    const regexQuery = RegExp(
      caseSensitive ? query : query.toLowerCase(),
      flags
    );
    const lineMatches = [];
    let singleMatch;

    while ((singleMatch = regexQuery.exec(lines[i])) !== null) {
      const startIndex = regexQuery.lastIndex;
      lineMatches.push(startIndex);
    }

    if (lineMatches.length !== 0) {
      const line = i + 1;
      const match = {
        ...data,
        label: type !== "url" ? line + "" : "Url",
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
 * @param modifiers
 * @param arr
 * @param data
 * @returns {*[]}
 */
function searchInArray(query, modifiers, arr, data) {
  const { key, label } = data;
  const matches = arr.map((match, i) =>
    findMatches(match, query, modifiers, {
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
 * @param query
 * @param modifiers
 * @param obj
 * @param data
 * @returns {*|[]}
 */
function searchInObject(query, modifiers, obj, data) {
  const matches = data.hasOwnProperty("collector") ? data.collector : [];
  const { caseSensitive } = modifiers;

  for (const objectKey in obj) {
    const objectKeyType = getType(obj[objectKey]);

    // if the value is an object, send to search in object
    if (objectKeyType === "object" && Object.keys(obj[objectKey].length > 1)) {
      searchInObject(query, obj[objectKey], {
        ...data,
        collector: matches,
      });

      continue;
    }

    const value = !caseSensitive
      ? obj[objectKey].toLowerCase()
      : obj[objectKey];
    const key = !caseSensitive ? objectKey.toLowerCase() : objectKey;
    const q = !caseSensitive ? query.toLowerCase() : query;

    if ((objectKeyType === "string" && value.includes(q)) || key.includes(q)) {
      const match = {
        ...data,
      };

      const startIndex = value.indexOf(q);

      match.label = objectKey;
      match.startIndex = startIndex;
      match.value = getTruncatedValue(obj[objectKey], query, startIndex);

      matches.push(match);
    }
  }

  return matches;
}
