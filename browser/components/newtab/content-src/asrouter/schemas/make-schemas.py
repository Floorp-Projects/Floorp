#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import sys
from argparse import ArgumentParser
from itertools import chain
from pathlib import Path

SCHEMA_BASE_DIR = Path("..", "templates")


MESSAGE_TYPES = {
    "CFRUrlbarChiclet": Path("CFR", "templates", "CFRUrlbarChiclet.schema.json"),
    "ExtensionDoorhanger": Path("CFR", "templates", "ExtensionDoorhanger.schema.json"),
    "InfoBar": Path("CFR", "templates", "InfoBar.schema.json"),
    "NewtabPromoMessage": Path("PBNewtab", "NewtabPromoMessage.schema.json"),
    "Spotlight": Path("OnboardingMessage", "Spotlight.schema.json"),
    "ToolbarBadgeMessage": Path("OnboardingMessage", "ToolbarBadgeMessage.schema.json"),
    "UpdateAction": Path("OnboardingMessage", "UpdateAction.schema.json"),
    "WhatsNewMessage": Path("OnboardingMessage", "WhatsNewMessage.schema.json"),
}


def read_schema(path):
    """Read a schema from disk and parse it as JSON."""
    with path.open("r") as f:
        return json.load(f)


def extract_template_values(template):
    """Extract the possible template values (either via JSON Schema enum or const)."""
    enum = template.get("enum")
    if enum:
        return enum

    const = template.get("const")
    if const:
        return [const]


def main(check=False):
    """Generate Nimbus feature schemas for Firefox Messaging System."""
    defs = {
        name: read_schema(SCHEMA_BASE_DIR / path)
        for name, path in MESSAGE_TYPES.items()
    }

    # Ensure all bundled schemas have an $id so that $refs inside the
    # bundled schema work correctly (i.e, they will reference the subschema
    # and not the bundle).
    for name, schema in defs.items():
        if "$id" not in schema:
            raise ValueError(f"Schema {name} is missing an $id")

        props = schema["properties"]
        if "template" not in props:
            raise ValueError(f"Schema {name} is missing a template")

        template = props["template"]
        if "enum" not in template and "const" not in template:
            raise ValueError(f"Schema {name} should have const or enum template")

    filename = Path("MessagingExperiment.schema.json")

    templates = {
        name: extract_template_values(schema["properties"]["template"])
        for name, schema in defs.items()
    }

    # Ensure that each schema has a unique set of template values.
    for a in templates.keys():
        a_keys = set(templates[a])

        for b in templates.keys():
            if a == b:
                continue

            b_keys = set(templates[b])
            intersection = a_keys.intersection(b_keys)

        if len(intersection):
            raise ValueError(
                f"Schema {a} and {b} have overlapping template values: {', '.join(intersection)}"
            )

    all_templates = list(chain.from_iterable(templates.values()))

    feature_schema = {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "$id": "resource://activity-stream/schemas/MessagingExperiment.schema.json",
        "title": "Messaging Experiment",
        "description": "A Firefox Messaging System message.",
        "allOf": [
            # Enforce that one of the templates must match (so that one of the
            # if branches will match).
            {
                "type": "object",
                "properties": {
                    "template": {
                        "type": "string",
                        "enum": all_templates,
                    },
                },
                "required": ["template"],
            },
            *(
                {
                    "if": {
                        "type": "object",
                        "properties": {
                            "template": {
                                "type": "string",
                                "enum": templates[message_type],
                            },
                        },
                        "required": ["template"],
                    },
                    "then": {"$ref": f"#/$defs/{message_type}"},
                }
                for message_type in defs
            ),
        ],
        "$defs": defs,
    }

    if check:
        print(f"Checking {filename} ...")

        with filename.open("r") as f:
            on_disk = json.load(f)

        if on_disk != feature_schema:
            print(f"{filename} does not match generated schema")
            print("Generated schema:")
            json.dump(feature_schema, sys.stdout)
            print("\n\nCommitted schema:")
            json.dump(on_disk, sys.stdout)

            raise ValueError("Schemas do not match!")

    else:
        with filename.open("wb") as f:
            print(f"Generating {filename} ...")
            f.write(json.dumps(feature_schema, indent=2).encode("utf-8"))
            f.write(b"\n")


if __name__ == "__main__":
    parser = ArgumentParser(description=main.__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check that the generated schemas have not changed",
        default=False,
    )
    args = parser.parse_args()

    main(args.check)
