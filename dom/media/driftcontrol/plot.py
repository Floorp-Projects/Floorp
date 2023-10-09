#! /usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This scripts plots graphs produced by our drift correction code.
#
# Install dependencies with:
#   > pip install bokeh pandas
#
# Generate the csv data file with the DriftControllerGraphs log module:
#   > MOZ_LOG=raw,sync,DriftControllerGraphs:5 \
#   > MOZ_LOG_FILE=/tmp/driftcontrol.csv       \
#   > ./mach gtest '*AudioDrift*StepResponse'
#
# Generate the graphs with this script:
#   > ./dom/media/driftcontrol/plot.py /tmp/driftcontrol.csv.moz_log
#
# The script should produce a file plot.html in the working directory and
# open it in the default browser.

import argparse
from collections import OrderedDict

import pandas
from bokeh.io import output_file, show
from bokeh.layouts import gridplot
from bokeh.models import TabPanel, Tabs
from bokeh.plotting import figure


def main():
    parser = argparse.ArgumentParser(
        prog="plot.py for DriftControllerGraphs",
        description="""Takes a csv file of DriftControllerGraphs data
(from a single DriftController instance) and plots
them into plot.html in the current working directory.

The easiest way to produce the data is with MOZ_LOG:
MOZ_LOG=raw,sync,DriftControllerGraphs:5 \
MOZ_LOG_FILE=/tmp/driftcontrol.csv       \
./mach gtest '*AudioDrift*StepResponse'""",
    )
    parser.add_argument("csv_file", type=str)
    args = parser.parse_args()

    all_df = pandas.read_csv(args.csv_file)

    # Filter on distinct ids to support multiple plotting sources
    tabs = []
    for id in list(OrderedDict.fromkeys(all_df["id"])):
        df = all_df[all_df["id"] == id]

        t = df["t"]
        buffering = df["buffering"]
        desired = df["desired"]
        buffersize = df["buffersize"]
        inlatency = df["inlatency"]
        outlatency = df["outlatency"]
        inrate = df["inrate"]
        outrate = df["outrate"]
        hysteresisthreshold = df["hysteresisthreshold"]
        corrected = df["corrected"]
        hysteresiscorrected = df["hysteresiscorrected"]
        configured = df["configured"]
        p = df["p"]
        i = df["i"]
        d = df["d"]
        kpp = df["kpp"]
        kii = df["kii"]
        kdd = df["kdd"]
        control = df["control"]

        output_file("plot.html")

        fig1 = figure()
        fig1.line(t, inlatency, color="hotpink", legend_label="In latency")
        fig1.line(t, outlatency, color="firebrick", legend_label="Out latency")
        fig1.line(t, buffering, color="dodgerblue", legend_label="Actual buffering")
        fig1.line(t, desired, color="goldenrod", legend_label="Desired buffering")
        fig1.line(t, buffersize, color="seagreen", legend_label="Buffer size")
        fig1.varea(
            t,
            [d - h for (d, h) in zip(desired, hysteresisthreshold)],
            [d + h for (d, h) in zip(desired, hysteresisthreshold)],
            alpha=0.2,
            color="goldenrod",
            legend_label="Hysteresis Threshold (won't correct out rate within area)",
        )

        fig2 = figure(x_range=fig1.x_range)
        fig2.line(t, inrate, color="hotpink", legend_label="Nominal in sample rate")
        fig2.line(t, outrate, color="firebrick", legend_label="Nominal out sample rate")
        fig2.line(
            t, corrected, color="dodgerblue", legend_label="Corrected out sample rate"
        )
        fig2.line(
            t,
            hysteresiscorrected,
            color="seagreen",
            legend_label="Hysteresis-corrected out sample rate",
        )
        fig2.line(
            t, configured, color="goldenrod", legend_label="Configured out sample rate"
        )

        fig3 = figure(x_range=fig1.x_range)
        fig3.line(t, p, color="goldenrod", legend_label="P")
        fig3.line(t, i, color="dodgerblue", legend_label="I")
        fig3.line(t, d, color="seagreen", legend_label="D")

        fig4 = figure(x_range=fig1.x_range)
        fig4.line(t, kpp, color="goldenrod", legend_label="KpP")
        fig4.line(t, kii, color="dodgerblue", legend_label="KiI")
        fig4.line(t, kdd, color="seagreen", legend_label="KdD")
        fig4.line(t, control, color="hotpink", legend_label="Control Signal")

        fig1.legend.location = "top_left"
        fig2.legend.location = "top_right"
        fig3.legend.location = "bottom_left"
        fig4.legend.location = "bottom_right"
        for fig in (fig1, fig2, fig3, fig4):
            fig.legend.background_fill_alpha = 0.6
            fig.legend.click_policy = "hide"

        tabs.append(
            TabPanel(child=gridplot([[fig1, fig2], [fig3, fig4]]), title=str(id))
        )

    show(Tabs(tabs=tabs))


if __name__ == "__main__":
    main()
