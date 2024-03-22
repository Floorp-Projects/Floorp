# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import click

from qm_try_analysis.analyze import analyze_qm_failures
from qm_try_analysis.fetch import fetch_qm_failures
from qm_try_analysis.report import report_qm_failures


@click.group(context_settings={"show_default": True})
def cli():
    pass


cli.add_command(fetch_qm_failures, "fetch")
cli.add_command(analyze_qm_failures, "analyze")
cli.add_command(report_qm_failures, "report")

if __name__ == "__main__":
    cli()
