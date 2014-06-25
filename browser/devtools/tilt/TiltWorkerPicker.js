/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This worker handles picking, given a set of vertices and a ray (calculates
 * the intersection points and offers back information about the closest hit).
 *
 * Used in the TiltVisualization.Presenter object.
 */
self.onmessage = function TWP_onMessage(event)
{
  let data = event.data;
  let vertices = data.vertices;
  let ray = data.ray;

  let intersection = null;
  let hit = [];

  // calculates the squared distance between two points
  function dsq(p1, p2) {
    let xd = p2[0] - p1[0];
    let yd = p2[1] - p1[1];
    let zd = p2[2] - p1[2];

    return xd * xd + yd * yd + zd * zd;
  }

  // check each stack face in the visualization mesh for intersections with
  // the mouse ray (using a ray picking algorithm)
  for (let i = 0, len = vertices.length; i < len; i += 36) {

    // the front quad
    let v0f = [vertices[i],      vertices[i + 1],  vertices[i + 2]];
    let v1f = [vertices[i + 3],  vertices[i + 4],  vertices[i + 5]];
    let v2f = [vertices[i + 6],  vertices[i + 7],  vertices[i + 8]];
    let v3f = [vertices[i + 9],  vertices[i + 10], vertices[i + 11]];

    // the back quad
    let v0b = [vertices[i + 24], vertices[i + 25], vertices[i + 26]];
    let v1b = [vertices[i + 27], vertices[i + 28], vertices[i + 29]];
    let v2b = [vertices[i + 30], vertices[i + 31], vertices[i + 32]];
    let v3b = [vertices[i + 33], vertices[i + 34], vertices[i + 35]];

    // don't do anything with degenerate quads
    if (!v0f[0] && !v1f[0] && !v2f[0] && !v3f[0]) {
      continue;
    }

    // for each triangle in the stack box, check for the intersections
    if (self.intersect(v0f, v1f, v2f, ray, hit) || // front left
        self.intersect(v0f, v2f, v3f, ray, hit) || // front right
        self.intersect(v0b, v1b, v1f, ray, hit) || // left back
        self.intersect(v0b, v1f, v0f, ray, hit) || // left front
        self.intersect(v3f, v2b, v3b, ray, hit) || // right back
        self.intersect(v3f, v2f, v2b, ray, hit) || // right front
        self.intersect(v0b, v0f, v3f, ray, hit) || // top left
        self.intersect(v0b, v3f, v3b, ray, hit) || // top right
        self.intersect(v1f, v1b, v2b, ray, hit) || // bottom left
        self.intersect(v1f, v2b, v2f, ray, hit)) { // bottom right

      // calculate the distance between the intersection hit point and camera
      let d = dsq(hit, ray.origin);

      // we're picking the closest stack in the mesh from the camera
      if (intersection === null || d < intersection.distance) {
        intersection = {
          // each mesh stack is composed of 12 vertices, so there's information
          // about a node once in 12 * 3 = 36 iterations (to avoid duplication)
          index: i / 36,
          distance: d
        };
      }
    }
  }

  self.postMessage(intersection);
  close();
};

/**
 * Utility function for finding intersections between a ray and a triangle.
 */
self.intersect = (function() {

  // creates a new instance of a vector
  function create() {
    return new Float32Array(3);
  }

  // performs a vector addition
  function add(aVec, aVec2, aDest) {
    aDest[0] = aVec[0] + aVec2[0];
    aDest[1] = aVec[1] + aVec2[1];
    aDest[2] = aVec[2] + aVec2[2];
    return aDest;
  }

  // performs a vector subtraction
  function subtract(aVec, aVec2, aDest) {
    aDest[0] = aVec[0] - aVec2[0];
    aDest[1] = aVec[1] - aVec2[1];
    aDest[2] = aVec[2] - aVec2[2];
    return aDest;
  }

  // performs a vector scaling
  function scale(aVec, aVal, aDest) {
    aDest[0] = aVec[0] * aVal;
    aDest[1] = aVec[1] * aVal;
    aDest[2] = aVec[2] * aVal;
    return aDest;
  }

  // generates the cross product of two vectors
  function cross(aVec, aVec2, aDest) {
    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];
    let x2 = aVec2[0];
    let y2 = aVec2[1];
    let z2 = aVec2[2];

    aDest[0] = y * z2 - z * y2;
    aDest[1] = z * x2 - x * z2;
    aDest[2] = x * y2 - y * x2;
    return aDest;
  }

  // calculates the dot product of two vectors
  function dot(aVec, aVec2) {
    return aVec[0] * aVec2[0] + aVec[1] * aVec2[1] + aVec[2] * aVec2[2];
  }

  let edge1 = create();
  let edge2 = create();
  let pvec = create();
  let tvec = create();
  let qvec = create();
  let lvec = create();

  // checks for ray-triangle intersections using the Fast Minimum-Storage
  // (simplified) algorithm by Tomas Moller and Ben Trumbore
  return function intersect(aVert0, aVert1, aVert2, aRay, aDest) {
    let dir = aRay.direction;
    let orig = aRay.origin;

    // find vectors for two edges sharing vert0
    subtract(aVert1, aVert0, edge1);
    subtract(aVert2, aVert0, edge2);

    // begin calculating determinant - also used to calculate the U parameter
    cross(dir, edge2, pvec);

    // if determinant is near zero, ray lines in plane of triangle
    let inv_det = 1 / dot(edge1, pvec);

    // calculate distance from vert0 to ray origin
    subtract(orig, aVert0, tvec);

    // calculate U parameter and test bounds
    let u = dot(tvec, pvec) * inv_det;
    if (u < 0 || u > 1) {
      return false;
    }

    // prepare to test V parameter
    cross(tvec, edge1, qvec);

    // calculate V parameter and test bounds
    let v = dot(dir, qvec) * inv_det;
    if (v < 0 || u + v > 1) {
      return false;
    }

    // calculate T, ray intersects triangle
    let t = dot(edge2, qvec) * inv_det;

    scale(dir, t, lvec);
    add(orig, lvec, aDest);
    return true;
  };
}());
