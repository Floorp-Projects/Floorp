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
# Generate the csv data file with the ClockDriftGraphs log module:
#   > MOZ_LOG=raw,sync,ClockDriftGraphs:5 \
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
        prog="plot.py for ClockDriftGraphs",
        description="""Takes a csv file of ClockDriftGraphs data
(from a single ClockDrift instance) and plots
them into plot.html in the current working directory.

The easiest way to produce the data is with MOZ_LOG:
MOZ_LOG=raw,sync,ClockDriftGraphs:5 \
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
        inrate = df["inrate"]
        outrate = df["outrate"]
        corrected = df["corrected"]

        output_file("plot.html")

        fig1 = figure()
        fig1.line(t, buffering, color="dodgerblue", legend_label="Actual buffering")
        fig1.line(t, desired, color="goldenrod", legend_label="Desired buffering")

        fig2 = figure(x_range=fig1.x_range)
        fig2.line(t, inrate, color="hotpink", legend_label="Nominal in sample rate")
        fig2.line(t, outrate, color="firebrick", legend_label="Nominal out sample rate")
        fig2.line(
            t, corrected, color="dodgerblue", legend_label="Corrected out sample rate"
        )

        fig1.legend.location = "top_left"
        fig2.legend.location = "top_right"
        for fig in (fig1, fig2):
            fig.legend.background_fill_alpha = 0.6
            fig.legend.click_policy = "hide"

        tabs.append(TabPanel(child=gridplot([[fig1, fig2]]), title=str(id)))

    show(Tabs(tabs=tabs))


if __name__ == "__main__":
    main()
