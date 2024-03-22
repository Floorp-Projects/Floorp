#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import hashlib
import json
import re
import sys
import webbrowser
from typing import Union

import bugzilla
import click
from click.utils import echo

from qm_try_analysis import stackanalysis, utils
from qm_try_analysis.logging import error, info, warning

# Flag for toggling development mod
DEV = True

# Constants for Bugzilla URLs
if DEV:
    BUGZILLA_BASE_URL = "https://bugzilla-dev.allizom.org/"
else:
    BUGZILLA_BASE_URL = "https://bugzilla.mozilla.org/"

BUGZILLA_API_URL = BUGZILLA_BASE_URL + "rest/"
BUGZILLA_ATTACHMENT_URL = BUGZILLA_BASE_URL + "attachment.cgi?id="
BUGZILLA_BUG_URL = BUGZILLA_BASE_URL + "show_bug.cgi?id="

# Constants for static bugs
QM_TRY_FAILURES_BUG = 1702411
WARNING_STACKS_BUG = 1711703

# Regex pattern for parsing anchor strings
ANCHOR_REGEX_PATTERN = re.compile(r"([^:]+):([^:]+)?:*([^:]+)?")


@click.command()
@click.option(
    "-k",
    "--key",
    help="Your personal Bugzilla API key",
    required=True,
)
@click.option(
    "--stacksfile",
    type=click.File("rb"),
    help="The output file of the previous analysis run. You only have to specify this, if the previous run does not include this info.",
)
@click.option(
    "--open-modified/--no-open-modified",
    default=True,
    help="Whether to open modified bugs in your default browser after updating them.",
)
@click.option(
    "-w",
    "--workdir",
    type=click.Path(file_okay=False, exists=True, writable=True),
    default="output",
    help="Working directory",
)
def report_qm_failures(key, stacksfile, open_modified, workdir):
    """
    Report QM failures to Bugzilla based on stack analysis.
    """
    run = utils.getLastRunFromExecutionFile(workdir)

    # Check for valid execution file from the previous run
    if not {"errorfile", "warnfile"} <= run.keys():
        error("No analyzable execution from the previous run of analyze found.")
        echo("Did you remember to run `poetry run qm-try-analysis analyze`?")
        sys.exit(2)

    # Handle missing stacksfile
    if not stacksfile:
        if "stacksfile" not in run:
            error(
                "The previous analyze run did not contain the location of the stacksfile."
            )
            echo('Please provide the file location using the "--stacksfile" option.')
            sys.exit(2)
        stacksfile = open(run["stacksfile"], "rb")

    # Create Bugzilla client
    bugzilla_client = bugzilla.Bugzilla(url=BUGZILLA_API_URL, api_key=key)

    # Initialize report data
    report = run.get("report", {})
    run["report"] = report
    attachment_id = report.get("stacksfile_attachment", None)
    reported = report.get("reported", [])
    report["reported"] = reported

    def post_comment(bug_id, comment):
        """
        Post a comment to a Bugzilla bug.
        """
        data = {"id": bug_id, "comment": comment, "is_markdown": True}
        res = bugzilla_client._post(f"bug/{bug_id}/comment", json.dumps(data))
        return res["id"]

    # Handle missing attachment ID
    if not attachment_id:
        attachment = bugzilla.DotDict()
        attachment.file_name = f"qmstacks_until_{run['lasteventtime']}.txt"
        attachment.summary = attachment.file_name
        attachment.content_type = "text/plain"
        attachment.data = stacksfile.read().decode()
        res = bugzilla_client.post_attachment(QM_TRY_FAILURES_BUG, attachment)
        attachment_id = next(iter(res["attachments"].values()))["id"]
        report["stacksfile_attachment"] = attachment_id
        utils.updateLastRunToExecutionFile(workdir, run)

        info(
            f'Created attachment for "{attachment.file_name}": {BUGZILLA_ATTACHMENT_URL + str(attachment_id)}.'
        )

    def generate_comment(stacks):
        """
        Generate a comment for Bugzilla based on error stacks.
        """
        comment = f"Taken from Attachment {attachment_id}\n\n"
        comment += stackanalysis.printStacks(stacks)
        return comment

    # Handle missing warnings comment
    if "warnings_comment" not in report:
        warning_stacks = utils.readJSONFile(run["warnfile"])
        warning_stacks = filter(lambda stack: stack["hit_count"] >= 100, warning_stacks)

        comment = generate_comment(warning_stacks)
        comment_id = post_comment(WARNING_STACKS_BUG, comment)

        report["warnings_comment"] = comment_id
        utils.updateLastRunToExecutionFile(workdir, run)

        info("Created comment for warning stacks.")

    error_stacks = utils.readJSONFile(run["errorfile"])

    def reduce(search_results, by: str) -> Union[int, None]:
        """
        Reduce bug search results automatically or based on user input.
        """
        anchor = by

        search_results = remove_duplicates(search_results, bugzilla_client)

        if not search_results:
            return

        if len(search_results) == 1:
            return search_results[0]["id"]

        echo(f'Multiple bugs found for anchor "{anchor}":')

        for i, result in enumerate(search_results, start=1):
            echo(
                f"{i}.{' [closed]' if result['resolution'] != '' else ''} {BUGZILLA_BUG_URL + str(result['id'])} - {result['summary']}"
            )

        choice = click.prompt(
            "Enter the number of the bug you want to use",
            type=click.Choice(
                [str(i) for i in range(1, len(search_results) + 1)] + ["skip"]
            ),
            default="skip",
            show_default=True,
            confirmation_prompt="Please confirm the selected choice",
        )

        if choice == "skip":
            return

        return search_results[int(choice) - 1]["id"]

    anchors = stackanalysis.groupStacksForAnchors(error_stacks)

    for anchor in anchors:
        if hash_str(anchor) in reported:
            info(f'Skipping anchor "{anchor}" since it has already been reported.')
            continue

        if not (match := ANCHOR_REGEX_PATTERN.match(anchor)):
            warning(f'"{anchor}" did not match the regex pattern.')

        if "Unknown" in match.group(2):
            warning(f'Skipping "{anchor}" since it is not a valid anchor.')
            continue

        search_string = " ".join(filter(None, match.groups()))
        search_results = bugzilla_client.search_bugs(
            [{"product": "Core", "summary": search_string}]
        )["bugs"]

        if bug_id := reduce(search_results, by=anchor):
            info(f'Found bug {BUGZILLA_BUG_URL + str(bug_id)} for anchor "{anchor}".')
        else:
            warning(f'No bug found for anchor "{anchor}".')

            if not click.confirm("Would you like to create one?"):
                continue

            bug = bugzilla.DotDict()
            bug.product = "Core"
            bug.component = "Storage: Quota Manager"
            bug.summary = f"[QM_TRY] Failures in {anchor}"
            bug.description = f"This bug keeps track of the semi-automatic monitoring of QM_TRY failures in `{anchor}`"
            bug["type"] = "defect"
            bug.blocks = QM_TRY_FAILURES_BUG
            bug.version = "unspecified"

            bug_id = bugzilla_client.post_bug(bug)["id"]

            info(f'Created bug {BUGZILLA_BUG_URL + str(bug_id)} for anchor "{anchor}".')

        comment = generate_comment(anchors[anchor]["stacks"])
        comment_id = post_comment(bug_id, comment)

        reported.append(hash_str(anchor))
        utils.updateLastRunToExecutionFile(workdir, run)

        if open_modified:
            comment_seq_number = bugzilla_client.get_comment(comment_id)["comments"][
                str(comment_id)
            ]["count"]
            webbrowser.open(
                BUGZILLA_BUG_URL + str(bug_id) + "#c" + str(comment_seq_number)
            )


def hash_str(s):
    """
    Hash a string using MD5.
    """
    encoded_str = s.encode("utf-8")
    return int(hashlib.md5(encoded_str).hexdigest(), 16)


def remove_duplicates(search_results, bugzilla_client):
    """
    Remove duplicate bugs in search results.
    """
    resolved_bugs = set(bug["id"] for bug in search_results if not bug.get("dupe_of"))

    def resolve_if_dupe(bug):
        if not (dupe_of := bug.get("dupe_of")):
            return bug

        if dupe_of in resolved_bugs:
            return None

        remote = resolve_if_dupe(bugzilla_client.get_bug(dupe_of))
        if remote:
            resolved_bugs.add(remote["id"])

        return remote

    return [non_dupe for bug in search_results if (non_dupe := resolve_if_dupe(bug))]


if __name__ == "__main__":
    report_qm_failures()
