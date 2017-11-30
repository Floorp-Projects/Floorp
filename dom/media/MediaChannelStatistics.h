/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaChannelStatistics_h_)
#define MediaChannelStatistics_h_

namespace mozilla {

// Number of bytes we have accumulated before we assume the connection download
// rate can be reliably calculated. 57 Segments at IW=3 allows slow start to
// reach a CWND of 30 (See bug 831998)
static const int64_t RELIABLE_DATA_THRESHOLD = 57 * 1460;

/**
 * This class is useful for estimating rates of data passing through
 * some channel. The idea is that activity on the channel "starts"
 * and "stops" over time. At certain times data passes through the
 * channel (usually while the channel is active; data passing through
 * an inactive channel is ignored). The GetRate() function computes
 * an estimate of the "current rate" of the channel, which is some
 * kind of average of the data passing through over the time the
 * channel is active.
 *
 * All methods take "now" as a parameter so the user of this class can
 * control the timeline used.
 */
class MediaChannelStatistics {
public:
  MediaChannelStatistics() = default;
  MediaChannelStatistics(const MediaChannelStatistics&) = default;
  MediaChannelStatistics& operator=(const MediaChannelStatistics&) = default;

  void Reset() {
    mLastStartTime = TimeStamp();
    mAccumulatedTime = TimeDuration(0);
    mAccumulatedBytes = 0;
    mIsStarted = false;
  }
  void Start() {
    if (mIsStarted)
      return;
    mLastStartTime = TimeStamp::Now();
    mIsStarted = true;
  }
  void Stop() {
    if (!mIsStarted)
      return;
    mAccumulatedTime += TimeStamp::Now() - mLastStartTime;
    mIsStarted = false;
  }
  void AddBytes(int64_t aBytes) {
    if (!mIsStarted) {
      // ignore this data, it may be related to seeking or some other
      // operation we don't care about
      return;
    }
    mAccumulatedBytes += aBytes;
  }
  double GetRateAtLastStop(bool* aReliable) {
    double seconds = mAccumulatedTime.ToSeconds();
    *aReliable = (seconds >= 1.0) ||
                 (mAccumulatedBytes >= RELIABLE_DATA_THRESHOLD);
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
  double GetRate(bool* aReliable) {
    TimeDuration time = mAccumulatedTime;
    if (mIsStarted) {
      time += TimeStamp::Now() - mLastStartTime;
    }
    double seconds = time.ToSeconds();
    *aReliable = (seconds >= 3.0) ||
                 (mAccumulatedBytes >= RELIABLE_DATA_THRESHOLD);
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
private:
  int64_t mAccumulatedBytes = 0;
  TimeDuration mAccumulatedTime;
  TimeStamp mLastStartTime;
  bool mIsStarted = false;
};

} // namespace mozilla

#endif // MediaChannelStatistics_h_
