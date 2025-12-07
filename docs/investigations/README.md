# Floorp Technical Investigations

This directory contains technical investigation documents for features, APIs, and compatibility improvements being considered for Floorp.

## Purpose

Investigation documents serve to:
- Research feasibility of new features or compatibility improvements
- Document technical approaches and alternatives
- Provide implementation recommendations
- Capture risks, limitations, and success metrics
- Serve as reference for future development decisions

## Current Investigations

### Active Investigations

- **[Offscreen API Support](offscreen-api-support.md)** ([日本語版](offscreen-api-support-ja.md))
  - Status: Investigation Complete
  - Priority: Medium
  - Summary: Investigation of Chrome Offscreen API support for improved Chrome extension compatibility
  - Recommendation: Implement polyfill using hidden iframes in Firefox MV3 background scripts

## Investigation Template

When creating a new investigation document, include the following sections:

1. **Executive Summary** - Brief overview of what is being investigated and why
2. **Background** - Context and motivation for the investigation
3. **Current State** - Where we are today
4. **Technical Analysis** - Detailed technical evaluation
5. **Options & Recommendations** - Different approaches with pros/cons
6. **Implementation Details** - How to implement the recommended approach
7. **Risks & Limitations** - What could go wrong or won't work
8. **Success Metrics** - How to measure success
9. **References** - Related documentation and resources
10. **Conclusion** - Summary and next steps

## File Naming Convention

Use kebab-case for file names:
- English: `feature-name.md`
- Japanese: `feature-name-ja.md`

## Contributing

When adding a new investigation:
1. Create the investigation document(s)
2. Update this README with a summary entry
3. Link to the investigation from relevant code comments using relative paths
4. Update the investigation status as work progresses

## Investigation Lifecycle

1. **Proposed** - Investigation is planned but not yet started
2. **In Progress** - Actively researching and documenting
3. **Investigation Complete** - Research finished, awaiting decision
4. **Approved** - Approved for implementation
5. **Implemented** - Feature has been implemented
6. **Archived** - Investigation closed without implementation
