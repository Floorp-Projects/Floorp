#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Firefox Messaging System Messaging Experiment schema generator

The Firefox Messaging System handles several types of messages. This program
patches and combines those schemas into a single schema
(MessagingExperiment.schema.json) which is used to validate messaging
experiments coming from Nimbus.

Definitions from FxMsCommon.schema.json are bundled into this schema. This
allows all of the FxMS schemas to reference common definitions, e.g.
`localizableText` for translatable strings, via referencing the common schema.
The bundled schema will be re-written so that the references now point at the
top-level, generated schema.

Additionally, all self-references in each messaging schema will be rewritten
into absolute references, referencing each sub-schemas `$id`. This is requried
due to the JSONSchema validation library used by Experimenter not fully
supporting self-references and bundled schema.
"""

import json
import sys
from argparse import ArgumentParser
from itertools import chain
from pathlib import Path
from urllib.parse import urlparse

import jsonschema


SCHEMA_BASE_DIR = Path("..", "templates")

MESSAGE_TYPES = {
    "CFRUrlbarChiclet": Path("CFR", "templates", "CFRUrlbarChiclet.schema.json"),
    "ExtensionDoorhanger": Path("CFR", "templates", "ExtensionDoorhanger.schema.json"),
    "InfoBar": Path("CFR", "templates", "InfoBar.schema.json"),
    "NewtabPromoMessage": Path("PBNewtab", "NewtabPromoMessage.schema.json"),
    "ProtectionsPanelMessage": Path(
        "OnboardingMessage", "ProtectionsPanelMessage.schema.json"
    ),
    "Spotlight": Path("OnboardingMessage", "Spotlight.schema.json"),
    "ToastNotification": Path("ToastNotification", "ToastNotification.schema.json"),
    "ToolbarBadgeMessage": Path("OnboardingMessage", "ToolbarBadgeMessage.schema.json"),
    "UpdateAction": Path("OnboardingMessage", "UpdateAction.schema.json"),
    "WhatsNewMessage": Path("OnboardingMessage", "WhatsNewMessage.schema.json"),
}

COMMON_SCHEMA_NAME = "FxMSCommon.schema.json"
COMMON_SCHEMA_PATH = Path(COMMON_SCHEMA_NAME)

TEST_CORPUS = {
    "CFRMessageProvider": Path("corpus", "CFRMessageProvider.messages.json"),
    "OnboardingMessageProvider": Path(
        "corpus", "OnboardingMessageProvider.messages.json"
    ),
    "PanelTestProvider": Path("corpus", "PanelTestProvider.messages.json"),
}

SCHEMA_ID = "resource://activity-stream/schemas/MessagingExperiment.schema.json"


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


def patch_schema(schema, schema_id=None):
    """Patch the given schema.

    The JSON schema validator that Experimenter uses
    (https://pypi.org/project/jsonschema/) does not support relative references,
    nor does it support bundled schemas. We rewrite the schema so that all
    relative refs are transformed into absolute refs via the schema's `$id`.

    Additionally, we merge in the contents of FxMSCommon.schema.json, so all
    refs relative to that schema will be transformed to become relative to this
    schema.

    See-also: https://github.com/python-jsonschema/jsonschema/issues/313
    """
    if schema_id is None:
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
            elif (uri.scheme, uri.path) == ("file", f"/{COMMON_SCHEMA_NAME}"):
                schema["$ref"] = f"{SCHEMA_ID}#{uri.fragment}"

        # If `schema` is object-like, inspect each of its indivual properties
        # and patch them.
        properties = schema.get("properties")
        if properties:
            for prop in properties.keys():
                patch_impl(properties[prop])

        # If `schema` is array-like, inspect each of its items and patch them.
        items = schema.get("items")
        if items:
            patch_impl(items)

        # Patch each `if`, `then`, `else`, and `not` sub-schema that is present.
        for key in ("if", "then", "else", "not"):
            if key in schema:
                patch_impl(schema[key])

        # Patch the items of each `oneOf`, `allOf`, and `anyOf` sub-schema that
        # is present.
        for key in ("oneOf", "allOf", "anyOf"):
            subschema = schema.get(key)
            if subschema:
                for i, alternate in enumerate(subschema):
                    patch_impl(alternate)

    # Patch the top-level type defined in the schema.
    patch_impl(schema)

    # Patch each named definition in the schema.
    for key in ("$defs", "definitions"):
        defns = schema.get(key)
        if defns:
            for defn_name, defn_value in defns.items():
                patch_impl(defn_value)

    return schema


def main(check=False):
    """Generate Nimbus feature schemas for Firefox Messaging System."""
    # Patch each message type schema to resolve all self-references to be
    # absolute and rewrite # references to FxMSCommon.schema.json to be relative
    # to the new schema (because we are about to bundle its definitions).
    defs = {
        name: patch_schema(read_schema(SCHEMA_BASE_DIR / path))
        for name, path in MESSAGE_TYPES.items()
    }

    # Bundle the definitions from FxMSCommon.schema.json into this schema.
    defs.update(
        patch_schema(read_schema(COMMON_SCHEMA_PATH), schema_id=SCHEMA_ID)["$defs"]
    )

    # Ensure all bundled schemas have an $id so that $refs inside the
    # bundled schema work correctly (i.e, they will reference the subschema
    # and not the bundle).
    for name in MESSAGE_TYPES.keys():
        schema = defs[name]
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
        name: extract_template_values(defs[name]["properties"]["template"])
        for name in MESSAGE_TYPES.keys()
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
                    f"Schema {a} and {b} have overlapping template values: "
                    f"{', '.join(intersection)}"
                )

    all_templates = list(chain.from_iterable(templates.values()))

    # Enforce that one of the templates must match (so that one of the if
    # branches will match).
    defs["Message"]["properties"]["template"]["enum"] = all_templates

    # Generate the combined schema.
    feature_schema = {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "$id": SCHEMA_ID,
        "title": "Messaging Experiment",
        "description": "A Firefox Messaging System message.",
        # A message must be one of
        # - an empty message (i.e., a completely empty object), which is the
        #   equivalent of an experiment branch not providing a message; or
        # - An object that contains a template field
        "oneOf": [
            {
                "description": "An empty FxMS message.",
                "type": "object",
                "additionalProperties": False,
            },
            {
                "allOf": [
                    # Ensure each message has all the fields defined in the base
                    # Message type.
                    #
                    # This is slightly redundant because each message should
                    # already inherit from this message type, but it is easier
                    # to add this requirement here than to verify that each
                    # message's schema is properly inheriting.
                    {"$ref": f"{SCHEMA_ID}#/$defs/Message"},
                    # For each message type, create a subschema that says if the
                    # template field matches a value for a message type defined
                    # in MESSAGE_TYPES, then the message must also match the
                    # schema for that message type.
                    #
                    # This is done using `allOf: [{ if, then }]` instead of `oneOf: []`
                    # because it provides better error messages. Using `if-then`
                    # will only show validation errors for the sub-schema that
                    # matches template, whereas using `oneOf` will show
                    # validation errors for *all* sub-schemas, which makes
                    # debugging messages much harder.
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
                            "then": {"$ref": f"{SCHEMA_ID}#/$defs/{message_type}"},
                        }
                        for message_type in MESSAGE_TYPES
                    ),
                ],
            },
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

    for provider, provider_path in TEST_CORPUS.items():
        print(f"  Validating messages from {provider}:")

        with provider_path.open("r") as f:
            messages = json.load(f)

        for message in messages:
            template = message["template"]
            msg_id = message["id"]

            print(f"    Validating {msg_id} {template} message with {filename}...")
            jsonschema.validate(instance=message, schema=schema, resolver=resolver)

        print()


if __name__ == "__main__":
    parser = ArgumentParser(description=main.__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check that the generated schemas have not changed and run validation tests.",
        default=False,
    )
    args = parser.parse_args()

    main(args.check)
