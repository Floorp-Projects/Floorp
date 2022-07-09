#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import sys
from argparse import ArgumentParser
from itertools import chain
from pathlib import Path
from urllib.parse import urlparse

import jsonschema


SCHEMA_BASE_DIR = Path("..", "templates")

PANEL_TEST_PROVIDER_MESSAGES = Path("PanelTestProvider.messages.json")

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


class NestedRefResolver(jsonschema.RefResolver):
    """A custom ref resolver that handles bundled schema.

    This is the resolver used by Experimenter.
    """

    def __init__(self, schema):
        super().__init__(base_uri=None, referrer=None)

        if "$id" in schema:
            self.store[schema["$id"]] = schema

        if "$defs" in schema:
            for dfn in schema["$defs"].values():
                if "$id" in dfn:
                    self.store[dfn["$id"]] = dfn


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


def patch_schema(schema):
    """Patch the given schema.

    The JSON schema validator that Experimenter uses
    (https://pypi.org/project/jsonschema/) does not support relative references,
    nor does it support bundled schemas. We rewrite the schema so that all
    relative refs are transformed into absolute refs via the schema's `$id`.

    See-also: https://github.com/python-jsonschema/jsonschema/issues/313
    """
    schema_id = schema["$id"]

    def patch_impl(schema):
        ref = schema.get("$ref")

        if ref:
            uri = urlparse(ref)
            if (
                uri.scheme == ""
                and uri.netloc == ""
                and uri.path == ""
                and uri.fragment != ""
            ):
                schema["$ref"] = f"{schema_id}#{uri.fragment}"

        properties = schema.get("properties")
        if properties:
            for prop in properties.keys():
                patch_impl(properties[prop])

        items = schema.get("items")
        if items:
            patch_impl(items)

        for key in ("if", "then", "else"):
            if key in schema:
                patch_impl(schema[key])

        for key in ("oneOf", "allOf", "anyOf"):
            subschema = schema.get(key)
            if subschema:
                for i, alternate in enumerate(subschema):
                    patch_impl(alternate)

    patch_impl(schema)

    for key in ("$defs", "definitions"):
        defns = schema.get(key)
        if defns:
            for defn_name, defn_value in defns.items():
                patch_impl(defn_value)

    return schema


def main(check=False):
    """Generate Nimbus feature schemas for Firefox Messaging System."""
    defs = {
        name: patch_schema(read_schema(SCHEMA_BASE_DIR / path))
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

    schema_id = "resource://activity-stream/schemas/MessagingExperiment.schema.json"

    feature_schema = {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "$id": schema_id,
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
                    "then": {"$ref": f"{schema_id}#/$defs/{message_type}"},
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

        check_schema(filename, schema=on_disk)

    else:
        with filename.open("wb") as f:
            print(f"Generating {filename} ...")
            f.write(json.dumps(feature_schema, indent=2).encode("utf-8"))
            f.write(b"\n")


def check_schema(filename, schema):
    """Check that the schema validates.

    This uses the same validation configuration that is used in Experimenter.
    """
    print("Validating messages with Experimenter JSON Schema validator...")

    resolver = NestedRefResolver(schema)

    with PANEL_TEST_PROVIDER_MESSAGES.open("r") as f:
        messages = json.load(f)

    for message in messages:
        # PanelTestProvider overrides the targeting of all messages. Some
        # messages are missing targeting and will fail without this patch.
        message["targeting"] = 'providerCohorts.panel_local_testing == "SHOW_TEST"'
        msg_id = message["id"]

        print(f"Validating {msg_id} with {filename}...")
        jsonschema.validate(instance=message, schema=schema, resolver=resolver)


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
