# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import click


def debug(message):
    click.echo(click.style("Debug", fg="cyan") + f": {message}")


def info(message):
    click.echo(click.style("Info", fg="white") + f": {message}")


def warning(message):
    click.echo(click.style("Warning", fg="yellow") + f": {message}")


def error(message):
    click.echo(click.style("Error", fg="red") + f": {message}")
