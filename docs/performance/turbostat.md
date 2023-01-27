# Turbostat

`turbostat` is a Linux command-line utility that prints various
measurements, including numerous per-CPU measurements. This article
provides an introduction to using it.

**Note**: The [power profiling overview](power_profiling_overview.md) is
worth reading at this point if you haven't already. It may make parts
of this document easier to understand.

## Invocation

`turbostat` must be invoked as the super-user:

```bash
sudo turbostat
```

If you get an error saying `"turbostat: no /dev/cpu/0/msr"`, you need to
run the following command:

```bash
sudo modprobe msr
```

The output is as follows:

```
    Core     CPU Avg_MHz   %Busy Bzy_MHz TSC_MHz     SMI  CPU%c1  CPU%c3  CPU%c6  CPU%c7 CoreTmp  PkgTmp Pkg%pc2 Pkg%pc3 Pkg%pc6 Pkg%pc7 PkgWatt CorWatt GFXWatt
       -       -     799   21.63    3694    3398       0   12.02    3.16    1.71   61.48      49      49    0.00    0.00    0.00    0.00   22.68   15.13    1.13
       0       0     821   22.44    3657    3398       0    9.92    2.43    2.25   62.96      39      49    0.00    0.00    0.00    0.00   22.68   15.13    1.13
       0       4     708   19.14    3698    3398       0   13.22
       1       1     743   20.26    3666    3398       0   21.40    4.01    1.42   52.90      49
       1       5    1206   31.98    3770    3398       0    9.69
       2       2     784   21.29    3681    3398       0   11.78    3.10    1.13   62.70      40
       2       6     782   21.15    3698    3398       0   11.92
       3       3     702   19.14    3670    3398       0    8.39    3.09    2.03   67.36      39
       3       7     648   17.67    3667    3398       0    9.85
```

The man page has good explanations of what each column measures. The
various "Watt" measurements come from the Intel RAPL MSRs.

If you run with the `-S` option you get a smaller range of measurements
that fit on a single line, like the following:

```
 Avg_MHz   %Busy Bzy_MHz TSC_MHz     SMI  CPU%c1  CPU%c3  CPU%c6  CPU%c7 CoreTmp  PkgTmp Pkg%pc2 Pkg%pc3 Pkg%pc6 Pkg%pc7 PkgWatt CorWatt GFXWatt
    3614   97.83    3694    3399       0    2.17    0.00    0.00    0.00      77      77    0.00    0.00    0.00    0.00   67.50   57.77    0.46
```
