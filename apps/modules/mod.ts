import { expandGlob } from "@jsr/std__fs/expand-glob";
import {relative} from "pathe";
export async function getEntries(cwd:string) {
  const entry: string[] = [];
  for await (
    const x of expandGlob(import.meta.dirname+"/**/*.mts")
  ) {
    entry.push(relative(cwd,x.path));
  }
  return entry;
}
