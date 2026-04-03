// SPDX-License-Identifier: MPL-2.0



export interface ServerFunctions {
  onDOMContentLoaded(href: string | undefined): void;
}

// deno-lint-ignore no-empty-interface
export interface ClientFunctions {}
