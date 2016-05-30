// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "IIRFilter.h"

#include <complex>

namespace blink {

// The length of the memory buffers for the IIR filter.  This MUST be a power of two and must be
// greater than the possible length of the filter coefficients.
const int kBufferLength = 32;
static_assert(kBufferLength >= IIRFilter::kMaxOrder + 1,
    "Internal IIR buffer length must be greater than maximum IIR Filter order.");

IIRFilter::IIRFilter(const AudioDoubleArray* feedforwardCoef, const AudioDoubleArray* feedbackCoef)
    : m_bufferIndex(0)
    , m_feedback(feedbackCoef)
    , m_feedforward(feedforwardCoef)
{
    m_xBuffer.SetLength(kBufferLength);
    m_yBuffer.SetLength(kBufferLength);
    reset();
}

IIRFilter::~IIRFilter()
{
}

void IIRFilter::reset()
{
    memset(m_xBuffer.Elements(), 0, m_xBuffer.Length() * sizeof(double));
    memset(m_yBuffer.Elements(), 0, m_yBuffer.Length() * sizeof(double));
}

static std::complex<double> evaluatePolynomial(const double* coef, std::complex<double> z, int order)
{
    // Use Horner's method to evaluate the polynomial P(z) = sum(coef[k]*z^k, k, 0, order);
    std::complex<double> result = 0;

    for (int k = order; k >= 0; --k)
        result = result * z + std::complex<double>(coef[k]);

    return result;
}

void IIRFilter::process(const float* sourceP, float* destP, size_t framesToProcess)
{
    // Compute
    //
    //   y[n] = sum(b[k] * x[n - k], k = 0, M) - sum(a[k] * y[n - k], k = 1, N)
    //
    // where b[k] are the feedforward coefficients and a[k] are the feedback coefficients of the
    // filter.

    // This is a Direct Form I implementation of an IIR Filter.  Should we consider doing a
    // different implementation such as Transposed Direct Form II?
    const double* feedback = m_feedback->Elements();
    const double* feedforward = m_feedforward->Elements();

    MOZ_ASSERT(feedback);
    MOZ_ASSERT(feedforward);

    // Sanity check to see if the feedback coefficients have been scaled appropriately. It must
    // be EXACTLY 1!
    MOZ_ASSERT(feedback[0] == 1);

    int feedbackLength = m_feedback->Length();
    int feedforwardLength = m_feedforward->Length();
    int minLength = std::min(feedbackLength, feedforwardLength);

    double* xBuffer = m_xBuffer.Elements();
    double* yBuffer = m_yBuffer.Elements();

    for (size_t n = 0; n < framesToProcess; ++n) {
        // To help minimize roundoff, we compute using double's, even though the filter coefficients
        // only have single precision values.
        double yn = feedforward[0] * sourceP[n];

        // Run both the feedforward and feedback terms together, when possible.
        for (int k = 1; k < minLength; ++k) {
            int n = (m_bufferIndex - k) & (kBufferLength - 1);
            yn += feedforward[k] * xBuffer[n];
            yn -= feedback[k] * yBuffer[n];
        }

        // Handle any remaining feedforward or feedback terms.
        for (int k = minLength; k < feedforwardLength; ++k)
            yn += feedforward[k] * xBuffer[(m_bufferIndex - k) & (kBufferLength - 1)];

        for (int k = minLength; k < feedbackLength; ++k)
            yn -= feedback[k] * yBuffer[(m_bufferIndex - k) & (kBufferLength - 1)];

        // Save the current input and output values in the memory buffers for the next output.
        m_xBuffer[m_bufferIndex] = sourceP[n];
        m_yBuffer[m_bufferIndex] = yn;

        m_bufferIndex = (m_bufferIndex + 1) & (kBufferLength - 1);

        // Avoid introducing a stream of subnormals
        // TODO: Remove this code when Bug 1157635 is fixed.
        if (fabs(yn) >= FLT_MIN) {
            destP[n] = yn;
        } else {
            destP[n] = 0.0;
        }
    }
}

void IIRFilter::getFrequencyResponse(int nFrequencies, const float* frequency, float* magResponse, float* phaseResponse)
{
    // Evaluate the z-transform of the filter at the given normalized frequencies from 0 to 1. (One
    // corresponds to the Nyquist frequency.)
    //
    // The z-tranform of the filter is
    //
    // H(z) = sum(b[k]*z^(-k), k, 0, M) / sum(a[k]*z^(-k), k, 0, N);
    //
    // The desired frequency response is H(exp(j*omega)), where omega is in [0, 1).
    //
    // Let P(x) = sum(c[k]*x^k, k, 0, P) be a polynomial of order P.  Then each of the sums in H(z)
    // is equivalent to evaluating a polynomial at the point 1/z.

    for (int k = 0; k < nFrequencies; ++k) {
        // zRecip = 1/z = exp(-j*frequency)
        double omega = -M_PI * frequency[k];
        std::complex<double> zRecip = std::complex<double>(cos(omega), sin(omega));

        std::complex<double> numerator = evaluatePolynomial(m_feedforward->Elements(), zRecip, m_feedforward->Length() - 1);
        std::complex<double> denominator = evaluatePolynomial(m_feedback->Elements(), zRecip, m_feedback->Length() - 1);
        std::complex<double> response = numerator / denominator;
        magResponse[k] = static_cast<float>(abs(response));
        phaseResponse[k] = static_cast<float>(atan2(imag(response), real(response)));
    }
}

bool IIRFilter::buffersAreZero()
{
    double* xBuffer = m_xBuffer.Elements();
    double* yBuffer = m_yBuffer.Elements();

    for (size_t k = 0; k < m_feedforward->Length(); ++k) {
        if (xBuffer[(m_bufferIndex - k) & (kBufferLength - 1)] != 0.0) {
            return false;
        }
    }

    for (size_t k = 0; k < m_feedback->Length(); ++k) {
        if (fabs(yBuffer[(m_bufferIndex - k) & (kBufferLength - 1)]) >= FLT_MIN) {
            return false;
        }
    }

    return true;
}

} // namespace blink
