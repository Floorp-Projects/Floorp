/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidVelocityTracker.h"

#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

// This velocity tracker implementation was adapted from Chromium's
// second-order unweighted least-squares velocity tracker strategy
// (https://cs.chromium.org/chromium/src/ui/events/gesture_detection/velocity_tracker.cc?l=101&rcl=9ea9a086d4f54c702ec9a38e55fb3eb8bbc2401b).

// Threshold between position updates for determining that a pointer has
// stopped moving. Some input devices do not send move events in the
// case where a pointer has stopped.  We need to detect this case so that we can
// accurately predict the velocity after the pointer starts moving again.
static const int kAssumePointerMoveStoppedTimeMs = 40;

// The degree of the approximation.
static const uint8_t kDegree = 2;

// The degree of the polynomial used in SolveLeastSquares().
// This should be the degree of the approximation plus one.
static const uint8_t kPolyDegree = kDegree + 1;

// Maximum size of position history.
static const uint8_t kHistorySize = 20;

AndroidVelocityTracker::AndroidVelocityTracker()
    : mLastEventTime(0), mAdditionalDelta(0) {}

void AndroidVelocityTracker::StartTracking(ParentLayerCoord aPos,
                                           uint32_t aTimestampMs) {
  Clear();
  mHistory.AppendElement(std::make_pair(aTimestampMs, aPos));
  mLastEventTime = aTimestampMs;
}

Maybe<float> AndroidVelocityTracker::AddPosition(ParentLayerCoord aPos,
                                                 uint32_t aTimestampMs) {
  if ((aTimestampMs - mLastEventTime) >= kAssumePointerMoveStoppedTimeMs) {
    Clear();
  }

  aPos += mAdditionalDelta;

  if (aTimestampMs == mLastEventTime) {
    // If we get a sample with the same timestamp as the previous one,
    // just update its position. Two samples in the history with the
    // same timestamp can lead to things like infinite velocities.
    if (mHistory.Length() > 0) {
      mHistory[mHistory.Length() - 1].second = aPos;
    }
  } else {
    mHistory.AppendElement(std::make_pair(aTimestampMs, aPos));
    if (mHistory.Length() > kHistorySize) {
      mHistory.RemoveElementAt(0);
    }
  }

  mLastEventTime = aTimestampMs;

  if (mHistory.Length() < 2) {
    return Nothing();
  }

  auto start = mHistory[mHistory.Length() - 2];
  auto end = mHistory[mHistory.Length() - 1];
  return Some((end.second - start.second) / (end.first - start.first));
}

float AndroidVelocityTracker::HandleDynamicToolbarMovement(
    uint32_t aStartTimestampMs, uint32_t aEndTimestampMs,
    ParentLayerCoord aDelta) {
  // If the dynamic toolbar is moving, the page content is moving relative
  // to the screen. The positions passed to AddPosition() reflect the position
  // of the finger relative to the page content, but we want the velocity we
  // compute to be based on the physical movement of the finger (that is, its
  // position relative to the screen). To accomplish this, we maintain
  // |mAdditionalDelta|, a delta representing the amount by which the page has
  // moved relative to the screen, and add it to every position recorded in
  // the history in AddPosition().
  mAdditionalDelta += aDelta;

  float timeDelta = aEndTimestampMs - aStartTimestampMs;
  MOZ_ASSERT(timeDelta != 0);
  return aDelta / timeDelta;
}

static float VectorDot(const float* a, const float* b, uint32_t m) {
  float r = 0;
  while (m--) {
    r += *(a++) * *(b++);
  }
  return r;
}

static float VectorNorm(const float* a, uint32_t m) {
  float r = 0;
  while (m--) {
    float t = *(a++);
    r += t * t;
  }
  return sqrtf(r);
}

/**
 * Solves a linear least squares problem to obtain a N degree polynomial that
 * fits the specified input data as nearly as possible.
 *
 * Returns true if a solution is found, false otherwise.
 *
 * The input consists of two vectors of data points X and Y with indices 0..m-1
 * along with a weight vector W of the same size.
 *
 * The output is a vector B with indices 0..n that describes a polynomial
 * that fits the data, such the sum of W[i] * W[i] * abs(Y[i] - (B[0] + B[1]
 * X[i] * + B[2] X[i]^2 ... B[n] X[i]^n)) for all i between 0 and m-1 is
 * minimized.
 *
 * Accordingly, the weight vector W should be initialized by the caller with the
 * reciprocal square root of the variance of the error in each input data point.
 * In other words, an ideal choice for W would be W[i] = 1 / var(Y[i]) = 1 /
 * stddev(Y[i]).
 * The weights express the relative importance of each data point.  If the
 * weights are* all 1, then the data points are considered to be of equal
 * importance when fitting the polynomial.  It is a good idea to choose weights
 * that diminish the importance of data points that may have higher than usual
 * error margins.
 *
 * Errors among data points are assumed to be independent.  W is represented
 * here as a vector although in the literature it is typically taken to be a
 * diagonal matrix.
 *
 * That is to say, the function that generated the input data can be
 * approximated by y(x) ~= B[0] + B[1] x + B[2] x^2 + ... + B[n] x^n.
 *
 * The coefficient of determination (R^2) is also returned to describe the
 * goodness of fit of the model for the given data.  It is a value between 0
 * and 1, where 1 indicates perfect correspondence.
 *
 * This function first expands the X vector to a m by n matrix A such that
 * A[i][0] = 1, A[i][1] = X[i], A[i][2] = X[i]^2, ..., A[i][n] = X[i]^n, then
 * multiplies it by w[i].
 *
 * Then it calculates the QR decomposition of A yielding an m by m orthonormal
 * matrix Q and an m by n upper triangular matrix R.  Because R is upper
 * triangular (lower part is all zeroes), we can simplify the decomposition into
 * an m by n matrix Q1 and a n by n matrix R1 such that A = Q1 R1.
 *
 * Finally we solve the system of linear equations given by
 * R1 B = (Qtranspose W Y) to find B.
 *
 * For efficiency, we lay out A and Q column-wise in memory because we
 * frequently operate on the column vectors.  Conversely, we lay out R row-wise.
 *
 * http://en.wikipedia.org/wiki/Numerical_methods_for_linear_least_squares
 * http://en.wikipedia.org/wiki/Gram-Schmidt
 */
static bool SolveLeastSquares(const float* x, const float* y, const float* w,
                              uint32_t m, uint32_t n, float* out_b) {
  // MSVC does not support variable-length arrays (used by the original Android
  // implementation of this function).
#if defined(COMPILER_MSVC)
  const uint32_t M_ARRAY_LENGTH = VelocityTracker::kHistorySize;
  const uint32_t N_ARRAY_LENGTH = VelocityTracker::kPolyDegree;
  DCHECK_LE(m, M_ARRAY_LENGTH);
  DCHECK_LE(n, N_ARRAY_LENGTH);
#else
  const uint32_t M_ARRAY_LENGTH = m;
  const uint32_t N_ARRAY_LENGTH = n;
#endif

  // Expand the X vector to a matrix A, pre-multiplied by the weights.
  float a[N_ARRAY_LENGTH][M_ARRAY_LENGTH];  // column-major order
  for (uint32_t h = 0; h < m; h++) {
    a[0][h] = w[h];
    for (uint32_t i = 1; i < n; i++) {
      a[i][h] = a[i - 1][h] * x[h];
    }
  }

  // Apply the Gram-Schmidt process to A to obtain its QR decomposition.

  // Orthonormal basis, column-major order.
  float q[N_ARRAY_LENGTH][M_ARRAY_LENGTH];
  // Upper triangular matrix, row-major order.
  float r[N_ARRAY_LENGTH][N_ARRAY_LENGTH];
  for (uint32_t j = 0; j < n; j++) {
    for (uint32_t h = 0; h < m; h++) {
      q[j][h] = a[j][h];
    }
    for (uint32_t i = 0; i < j; i++) {
      float dot = VectorDot(&q[j][0], &q[i][0], m);
      for (uint32_t h = 0; h < m; h++) {
        q[j][h] -= dot * q[i][h];
      }
    }

    float norm = VectorNorm(&q[j][0], m);
    if (norm < 0.000001f) {
      // vectors are linearly dependent or zero so no solution
      return false;
    }

    float invNorm = 1.0f / norm;
    for (uint32_t h = 0; h < m; h++) {
      q[j][h] *= invNorm;
    }
    for (uint32_t i = 0; i < n; i++) {
      r[j][i] = i < j ? 0 : VectorDot(&q[j][0], &a[i][0], m);
    }
  }

  // Solve R B = Qt W Y to find B.  This is easy because R is upper triangular.
  // We just work from bottom-right to top-left calculating B's coefficients.
  float wy[M_ARRAY_LENGTH];
  for (uint32_t h = 0; h < m; h++) {
    wy[h] = y[h] * w[h];
  }
  for (uint32_t i = n; i-- != 0;) {
    out_b[i] = VectorDot(&q[i][0], wy, m);
    for (uint32_t j = n - 1; j > i; j--) {
      out_b[i] -= r[i][j] * out_b[j];
    }
    out_b[i] /= r[i][i];
  }

  return true;
}

Maybe<float> AndroidVelocityTracker::ComputeVelocity(uint32_t aTimestampMs) {
  if (mHistory.IsEmpty()) {
    return Nothing{};
  }

  // Polynomial coefficients describing motion along the axis.
  float xcoeff[kPolyDegree + 1];
  for (size_t i = 0; i <= kPolyDegree; i++) {
    xcoeff[i] = 0;
  }

  // Iterate over movement samples in reverse time order and collect samples.
  float pos[kHistorySize];
  float w[kHistorySize];
  float time[kHistorySize];
  uint32_t m = 0;
  int index = mHistory.Length() - 1;
  const uint32_t horizon = gfxPrefs::APZVelocityRelevanceTime();
  const auto& newest_movement = mHistory[index];

  do {
    const auto& movement = mHistory[index];
    uint32_t age = newest_movement.first - movement.first;
    if (age > horizon) break;

    ParentLayerCoord position = movement.second;
    pos[m] = position;
    w[m] = 1.0f;
    time[m] = -static_cast<float>(age) / 1000.0f;  // in seconds
    index--;
    m++;
  } while (index >= 0);

  if (m == 0) {
    return Nothing{};  // no data
  }

  // Calculate a least squares polynomial fit.

  // Polynomial degree (number of coefficients), or zero if no information is
  // available.
  uint32_t degree = kDegree;
  if (degree > m - 1) {
    degree = m - 1;
  }

  if (degree >= 1) {  // otherwise, no velocity data available
    uint32_t n = degree + 1;
    if (SolveLeastSquares(time, pos, w, m, n, xcoeff)) {
      float velocity = xcoeff[1];

      // The velocity needs to be negated because the positions represent
      // touch positions, and the direction of scrolling is opposite to the
      // direction of the finger's movement.
      return Some(-velocity / 1000.0f);  // convert to pixels per millisecond
    }
  }

  return Nothing{};
}

void AndroidVelocityTracker::Clear() {
  mAdditionalDelta = 0;
  mHistory.Clear();
}

}  // namespace layers
}  // namespace mozilla
