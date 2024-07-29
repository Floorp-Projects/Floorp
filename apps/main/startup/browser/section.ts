/**
 * make base section for noraneko content that inserted to target.before
 * @param targetId the target id that the section be inserted in before
 * @output the section's id that will be made is `@nora:gecko:${targetId}`
 */
export function appendSectionBefore(targetId: string) {
  const target = document.getElementById(targetId);
  if (target === null) {
    throw Error(`@nora:startup:appendSection | the #${targetId} not found`);
  }

  const section = document.createElement("section");
  section.id = `@nora:gecko:${targetId}`;
  target.before(section);
}
