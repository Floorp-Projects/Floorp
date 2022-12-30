# PerfStats

PerfStats is a framework for the low-overhead selective collection of internal performance metrics.
The results are accessible through ChromeUtils, Browsertime output, and in select performance tests.

## Adding a new PerfStat
Define the new PerfStat by adding it to [this list](https://searchfox.org/mozilla-central/rev/b1e5f2c7c96be36974262551978d54f457db2cae/tools/performance/PerfStats.h#34-53) in [`PerfStats.h`](https://searchfox.org/mozilla-central/rev/52da19becaa3805e7f64088e91e9dade7dec43c8/tools/performance/PerfStats.h).
Then, in C++ code, wrap execution in an RAII object, e.g.
```
PerfStats::AutoMetricRecording<PerfStats::Metric::MyMetric>()
```
or call the following function manually:
```
PerfStats::RecordMeasurement(PerfStats::Metric::MyMetric, Start, End)
```
For incrementing counters, use the following:
```
PerfStats::RecordMeasurementCount(PerfStats::Metric::MyMetric, incrementCount)
```

[Here's an example of a patch where a new PerfStat was added and used.](https://hg.mozilla.org/mozilla-central/rev/3e85a73d1fa5c816fdaead66ecee603b38f9b725)

## Enabling collection
To enable collection, use `ChromeUtils.SetPerfStatsCollectionMask(MetricMask mask)`, where `mask=0` disables all metrics and `mask=0xFFFFFFFF` enables all of them.
`MetricMask` is a bitmask based on `Metric`, i.e. `Metric::LayerBuilding (2)` is synonymous to `1 << 2` in `MetricMask`.

## Accessing results
Results can be accessed with `ChromeUtils.CollectPerfStats()`.
The Browsertime test framework will sum results across processes and report them in its output.
The raptor-browsertime Windows essential pageload tests also collect all PerfStats.
